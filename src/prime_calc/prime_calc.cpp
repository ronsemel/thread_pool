#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

#include "thread_pool.h"

using namespace std;
using namespace ramp_up;

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
    for (auto i = 4; i <= n; ++i)
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

bool is_digits(const string &str)
{
    return all_of(str.begin(), str.end(), ::isdigit);
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        cerr << "Error: too few arguments" << endl;
        return 1;
    }

    else if (argc > 4)
    {
        cerr << "Error: too many arguments" << endl;
    }

    /// convert user input from strings to unsigned long long
    string thread_count_str(argv[1]);
    string lower_limit_str(argv[2]);
    string upper_limit_str(argv[3]);

    if (!(is_digits(thread_count_str)) || !(is_digits(lower_limit_str)) || !(is_digits(upper_limit_str)))
    {
        cerr << "Error: args may only be numeric" << endl;
    }

    int thread_count;
    int64_t lower_limit;
    int64_t upper_limit;
    try
    {
        thread_count = stoi(thread_count_str);
        lower_limit = stoll(lower_limit_str);
        upper_limit = stoll(upper_limit_str);
    }

    /// check if argument is invalid or out of range
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    if (thread_count < 0)
    {
        cerr << "Error: num of threads can only be positive" << endl;
        return 1;
    }

    thread_pool pool(thread_count);
    auto prime_counter = make_shared<atomic<int>>(0);
    for (auto i = lower_limit; i <= upper_limit; ++i)
    {
        pool.submit(count_prime_nums, i, prime_counter);
    }

    pool.wait_all();

    cout << "Between " << lower_limit << " and " << upper_limit << " there are: " << *prime_counter << " prime numbers" << endl;
    cout << "Caluclation took " << pool.elapsed_time().count() << " nanoseconds" << endl;

    return 0;
}
