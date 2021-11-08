#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/lexical_cast.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>

/// @brief An API for a thread pool where generic tasks(functions and there args) can be submitted and executed
/// @pre the tasks to be submitted to the pool must be <b>void functions</b>,
/// if you want a return value, send a pointer to the function.
/// @author Ran Semel 5.8.2021

namespace ramp_up
{
    class thread_pool
    {
    public:
        /**
        * @brief Construct a new thread pool object
        * 
        * @param num_of_threads the number of threads to initialize the pool with
        */
        explicit thread_pool(std::size_t num_of_threads);

        /// @brief destroys the threadpool and frees all allocated resources by it
        ~thread_pool();

        enum class task_status_t
        {
            WAITING,
            RUNNING,
            FINISHED
        };

        using task_id_t = boost::uuids::uuid;

        /**
         * @brief ubmits tasks to be executed by the thread pool
         * @pre the function submitted must be a <b>void function</b>
         * @param func the function to be executed 
         * @param args the arguments to be sent to the function
         * @code{.cpp}
         *  hread_pool pool(8);
         * int x = 5, y= 7;
         * int* res;
         * pool.submit(sum,x,y,res);
         * @endcode
         * @return task_id_t the uuid of the new task
         */
        template <typename Func, typename... Args>
        task_id_t submit(Func &&func, Args &&...args);

        /** @brief returns the status of a specific task
         *  @pre each task is assigned a boost::uuids::uuid
         * @return the status of the task: waiting, running or finished
         */
        task_status_t status(task_id_t id);

        /// @brief returns the elapsed time since the thread pool was instantiated
        std::chrono::duration<double, std::nano> elapsed_time();

        /// @brief a blocking call, waits until all tasks submitted to the pool are finished
        void wait_all();

        /// @brief a blocking call, waits until a task with a specific uuid is finished
        void wait(task_id_t id);

        thread_pool() = delete;
        thread_pool(const thread_pool &) = delete;
        thread_pool &operator=(const thread_pool &) = delete;

    private:
        using Task = std::function<void()>;
        using time_stamp_t = std::chrono::time_point<std::chrono::high_resolution_clock>;
        /// the num of threads in the pool
        int m_thread_count;
        /// time stamp from when the thread pool was instantiated
        time_stamp_t m_start_time;
        /// indicates whether threadpool shutdown procedure has initiated
        std::atomic<bool> m_need_shutdown;
        /// list of tasks waiting to be executed by the thread pool
        std::queue<std::pair<task_id_t, Task>> m_tasks_waiting;
        /// a Task can be WAITING, RUNNING or FINISHED
        std::unordered_map<std::string, task_status_t> m_task_id_to_status;
        /// vector of the working threads in the pool
        std::vector<std::thread> m_workers;
        /// manages access to shared variables in the critical sections
        std::mutex m_mutex;
        /// sends worker threads to waiting queue if the m_tasks_waiting list is empty
        std::condition_variable m_cond_tasks_waiting_or_need_shutdown;
        /// used in the wait_all() function, sends the main thread to waiting queue until m_tasks_waiting is empty
        std::condition_variable m_cond_all_finished;

        std::condition_variable m_cond_task_finished;
        ///uuid generator
        boost::uuids::random_generator uuid_generator;
        void startThreads();
        /// @brief joins all threads in thread pool
        void shutdown();

    }; //class thread_pool
}

template <typename Func, typename... Args>
ramp_up::thread_pool::task_id_t ramp_up::thread_pool::submit(Func &&f, Args &&...args)
{
    // create the task: bind the func and args into an std::function<void(void)>
    Task task = (std::bind(f, args...));
    // generate uuid for the task
    boost::uuids::uuid id = uuid_generator();
    //convert the uuid to string so we can use as a key in the hash tables as a key
    std::string id_str = boost::lexical_cast<std::string>(id);
    { //begin of critical section, locking mutex
        std::unique_lock<std::mutex> lock{m_mutex};
        m_tasks_waiting.push(std::make_pair(id, task));
        m_task_id_to_status.insert(std::make_pair(id_str, task_status_t::WAITING));
        //notify a thread that task list isn't empty
        m_cond_tasks_waiting_or_need_shutdown.notify_one();
    } //end of critical section, mutex unlocks at end of scope

    return id;
}
