#include <cstdlib>
#include <ctime>
#include <unordered_map>

#include <boost/test/unit_test.hpp>

#include "thread_pool.h"

#define MAX_UPPER_NUM 10'000
#define MAX_TASKS 1000
#define MAX_NUM 5000
#define NUM_OF_ROUNDS_TO_RUN 1

using namespace std;
using namespace boost::uuids;

namespace
{
    void add_sum(int64_t n1, int64_t n2, shared_ptr<atomic<int64_t>> res)
    {
        *res += n1 + n2;
    }

    void sleep_and_then_change_num(shared_ptr<atomic<int64_t>> num)
    {
        sleep(5);
        *num = 1;
    }

    void simulate_long_runtime()
    {
        sleep(3);
    }

    //function to test the thread_pool with
    bool is_num_prime(int64_t num)
    {
        //corner case: if m_num is 2 than it is prime
        if (num == 2 || num == 3)
        {
            return true;
        }

        //if m_num is less than 2 or divisible by 2 or 3, than it isn't a prime number
        if ((num <= 1) || (num % 2 == 0) || (num % 3 == 0))
        {
            return false;
        }

        int n = sqrt(num);
        for (int i = 2; i <= n; ++i)
        {
            //if m_num is divisible by 2 <= i <= sqrt(m_num), it isn't a prime number
            if (num % i == 0)
            {
                return false;
            }
        }

        return true;
    }

    void count_prime_nums(int64_t num, shared_ptr<atomic<int>> prime_counter)
    {
        if (is_num_prime(num))
        {
            (*prime_counter)++;
        }
    }
} //namespace

BOOST_AUTO_TEST_SUITE(ramp_up_tests)

BOOST_AUTO_TEST_CASE(sanity_check)
{
    ramp_up::thread_pool pool(4);
    auto prime_counter = make_shared<atomic<int>>(0);
    //check num of prime nums between 1 - 100 (correct ans = 25)
    int expected_primes = 25;
    for (auto i = 1; i <= 100; ++i)
    {
        pool.submit(count_prime_nums, i, prime_counter);
    }

    pool.wait_all();
    BOOST_TEST_MESSAGE("Calculating num of primes between 1 - 100. Expected 25");
    BOOST_CHECK_EQUAL(expected_primes, *prime_counter);
    //check num of prime nums between 1 - 10,000 (correct ans = 1,229)
    expected_primes = 1'229;
    *prime_counter = 0;
    for (auto i = 1; i <= 10'000; ++i)
    {
        pool.submit(count_prime_nums, i, prime_counter);
    }

    pool.wait_all();
    BOOST_TEST_MESSAGE("Calculating num of primes between 1 - 10,000. Expected 1,229");
    BOOST_CHECK_EQUAL(expected_primes, *prime_counter);

    //check num of prime nums between 1 - 100,000 (correct ans = 9,592)
    expected_primes = 9'592;
    *prime_counter = 0;
    for (auto i = 1; i <= 100'000; ++i)
    {
        pool.submit(count_prime_nums, i, prime_counter);
    }

    pool.wait_all();
    BOOST_TEST_MESSAGE("Calculating num of primes between 1 - 100,000. Expected 9,529");
    BOOST_CHECK_EQUAL(expected_primes, *prime_counter);

    //check num of prime nums between 1 - 1,000,000 (correct ans = 78,498)
    expected_primes = 78'498;
    *prime_counter = 0;
    for (auto i = 1; i <= 1'000'000; ++i)
    {
        pool.submit(count_prime_nums, i, prime_counter);
    }

    pool.wait_all();
    BOOST_TEST_MESSAGE("Calculating num of primes between 1 - 1,000,000. Expected 78,498");
    BOOST_CHECK_EQUAL(expected_primes, *prime_counter);
}

BOOST_AUTO_TEST_CASE(test_submit_task)
{
    srand(time(nullptr));
    ramp_up::thread_pool pool(8);
    //run this test NUM_OF_ROUNDS_TO_RUN times
    for (auto i = 1; i <= NUM_OF_ROUNDS_TO_RUN; ++i)
    {
        //generate rand num of tasks to submit
        int64_t num_of_tasks = rand() % MAX_TASKS;
        auto sum = make_shared<atomic<int64_t>>(0);
        int64_t expected_sum = 0;
        //submit the task, sum two rand nums and add them to result
        for (auto j = 1; j <= num_of_tasks; ++j)
        {
            int64_t n1 = rand() % MAX_NUM;
            int64_t n2 = rand() % MAX_NUM;
            pool.submit(add_sum, n1, n2, sum);
            expected_sum += n1 + n2;
        }
        //wait for tasks to be completed and compare them to healthy result
        pool.wait_all();
        BOOST_CHECK_EQUAL(expected_sum, *sum);
    }
}

BOOST_AUTO_TEST_CASE(test_wait)
{
    ramp_up::thread_pool pool(8);
    //first we set num to 0
    auto num = make_shared<atomic<int64_t>>(0);
    //now we will change it to one but it will sleep 10 seconds first
    auto id = pool.submit(sleep_and_then_change_num, num);
    //if wait doesn't work, num will still equal 0 when we compare num with 1
    pool.wait(id);
    BOOST_TEST_MESSAGE("Testing what pool.wait(id) waits for task with uuid = id to finish");
    BOOST_REQUIRE_MESSAGE(1, *num);
}

BOOST_AUTO_TEST_CASE(test_status)
{
    ramp_up::thread_pool pool(2);
    //submit 8 tasks to pool of 2 threads, each task takes 3 seconds to run, we will check that last task's status is WAITING
    boost::uuids::uuid id;
    for (auto i = 1; i <= 8; ++i)
    {
        id = pool.submit(simulate_long_runtime);
    }
    
    auto status = pool.status(id);
    BOOST_CHECK(status == ramp_up::thread_pool::task_status_t::WAITING);
    pool.wait_all();

    //submit task to pool. the task takes 3 secs to complete. we sleep 1 sec and then verify task is RUNNING
    id = pool.submit(simulate_long_runtime);
    sleep(1);
    status = pool.status(id);
    BOOST_CHECK(status == ramp_up::thread_pool::task_status_t::RUNNING);

    //wait for task to finish and check its status is FINISHED
    pool.wait(id);
    status = pool.status(id);
    BOOST_CHECK(status == ramp_up::thread_pool::task_status_t::FINISHED);

    //generate rand uuid and verify it was never submitted, meaning its status is FINISHED
    random_generator uuid_generator;
    auto rand_id = uuid_generator();
    status = pool.status(rand_id);
    BOOST_CHECK(status == ramp_up::thread_pool::task_status_t::FINISHED);
}

/**
 * @brief in this test we will check all of the threadpool's api's,
 * we will generate random lower_limits and random upper_limits,
 * we will calculate the healthy result and compare it with the actual result
 * 
 */
BOOST_AUTO_TEST_CASE(test_high_quantity_of_submitions)
{
    for (auto i = 1; i <= NUM_OF_ROUNDS_TO_RUN; ++i)
    {
        ramp_up::thread_pool pool(8);
        srand(time(nullptr));
        int64_t lower_limit = rand() % MAX_UPPER_NUM;
        int64_t upper_limit = rand() % MAX_UPPER_NUM;
        //if lower limit is larger than upper limit swap them
        if (upper_limit < lower_limit)
        {
            int64_t tmp = lower_limit;
            lower_limit = upper_limit;
            upper_limit = tmp;
        }

        int64_t expected_primes = 0;
        for (auto j = lower_limit; j <= upper_limit; ++j)
        {
            if (is_num_prime(j))
            {
                expected_primes++;
            }
        }

        auto prime_counter = make_shared<atomic<int>>(0);
        for (auto j = lower_limit; j <= upper_limit; ++j)
        {
            pool.submit(count_prime_nums, j, prime_counter);
        }

        pool.wait_all();
        BOOST_TEST_MESSAGE("Calculating num of primes between " << lower_limit << " and " << upper_limit << " Expected: " << expected_primes);
        BOOST_CHECK_EQUAL(expected_primes, *prime_counter);
    }
}

BOOST_AUTO_TEST_SUITE_END()