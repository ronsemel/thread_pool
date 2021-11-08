#include "thread_pool.h"

using namespace ramp_up;
using namespace std;

thread_pool::thread_pool(size_t num_of_threads) : m_thread_count(num_of_threads),
                                                  m_start_time(chrono::high_resolution_clock::now()),
                                                  m_need_shutdown(false)
{
    if (num_of_threads <= 0)
    {
        throw invalid_argument("Num of threads must be positive");
    }
    startThreads();
}

thread_pool::~thread_pool()
{
    shutdown();
}

thread_pool::task_status_t thread_pool::status(task_id_t id)
{
    unordered_map<string, task_status_t>::const_iterator iterator;
    //convert uuid to string in order to search the hash table with it
    string id_str = boost::lexical_cast<string>(id);
    { //begin of critical section, locking mutex
        unique_lock<mutex> lock{m_mutex};
        iterator = m_task_id_to_status.find(id_str);
        if (iterator == m_task_id_to_status.end())
        {
            return task_status_t::FINISHED;
        }
    } //end of critical section, mutex unlocks at end of scope

    return iterator->second;
}

void thread_pool::wait_all()
{
    {
        unique_lock<mutex> lock{m_mutex};
        m_cond_all_finished.wait(lock, [&]
                                { return m_tasks_waiting.empty(); });
    }
}

void thread_pool::wait(task_id_t id)
{
    {
        unique_lock<mutex> lock{m_mutex};
        m_cond_task_finished.wait(lock, [&]
                                  { return m_task_id_to_status.find(boost::lexical_cast<string>(id)) == m_task_id_to_status.end(); });
    }
}

void thread_pool::shutdown()
{
    m_need_shutdown = true;
    m_cond_tasks_waiting_or_need_shutdown.notify_all();
    for (auto &thread : m_workers)
    {
        thread.join();
    }
}

chrono::duration<double, nano> thread_pool::elapsed_time()
{
    auto curr_time = chrono::high_resolution_clock::now();
    return curr_time - m_start_time;
}

void thread_pool::startThreads()
{
    for (auto i = 1; i <= m_thread_count; ++i)
    {
        m_workers.emplace_back([&]
                                {
                                    while (true)
                                    { 
                                        pair<task_id_t, Task> task;
                                        { //begin of critical section, locking mutex
                                            unique_lock<mutex> lock{m_mutex};
                                            m_cond_tasks_waiting_or_need_shutdown.wait(lock, [=]
                                                                { return m_need_shutdown || !(m_tasks_waiting.empty()); });

                                            if (m_need_shutdown)
                                            {
                                                break;
                                            }

                                            //retrieve task from list
                                            task = move(m_tasks_waiting.front());
                                            //remove task from list
                                            m_tasks_waiting.pop();
                                            //update task status in hash to running
                                            m_task_id_to_status.insert_or_assign(boost::lexical_cast<string>(task.first), task_status_t::RUNNING);
                                        } //end of critical section, mutex unlocks at end of scope

                                        task.second(); //execute task

                                        { //begin of critical section, locking mutex
                                            unique_lock<mutex> lock{m_mutex};
                                            m_task_id_to_status.erase(boost::lexical_cast<string>(task.first));
                                            m_cond_task_finished.notify_all();
                                            if (m_tasks_waiting.empty()) //if list is empty notify main thread
                                            {
                                                m_cond_all_finished.notify_one();
                                            }
                                        } //end of critical section, mutex unlocks at end of scope
                                    }
                                });
    }
}
