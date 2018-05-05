#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <chrono>
#include <unistd.h>
#include "Threadpool.hpp"

using namespace std;
    // from https://stackoverflow.com/questions/26516683/reusing-thread-in-loop-c

    ThreadPool::ThreadPool(int threads) : shutdown_ (false) {
        // Create the specified number of threads
        threads_.reserve (threads);
        for (int i = 0; i < threads; ++i) {
            threads_.emplace_back(bind(&ThreadPool::threadEntry, this, i));
        }
    }

    ThreadPool::~ThreadPool() {
        {
            // Unblock any threads and tell them to stop
            unique_lock<mutex> l(lock_);

            shutdown_ = true;
            condVar_.notify_all();
        }

        // Wait for all threads to stop
        cerr << "Joining threads" << endl;
        for (auto& thread : threads_)
            thread.join();
    }

    void ThreadPool::doJob(function<void(void)> func) {
        // Place a job on the queu and unblock a thread
        unique_lock<mutex> l(lock_);

        jobs_.emplace(move(func));
        condVar_.notify_one();
    }

    void ThreadPool::threadEntry(int i) {
        function<void(void)> job;

        while (1) {
            {
                unique_lock<mutex> l(lock_);

                while (!shutdown_ && jobs_.empty()) {
                    condVar_.wait(l);
                }

                if (jobs_.empty ()) {
                    // No jobs to do and we are shutting down
                    cerr << "Thread " << i << " terminates" << endl;
                    return;
                 }

                cerr << "Thread " << i << " does a job" << endl;
                job = move(jobs_.front());
                jobs_.pop();
            }

            // Do the job without holding any locks
            job();
        }
    }

void silly (int n) {
    // A silly job for demonstration purposes
    cerr << "Sleeping for " << n << " seconds" << endl;
    this_thread::sleep_for(chrono::seconds(n));
}

int main() {
    ThreadPool pool(4);

    for (int i = 0; i < 100; i++) {
        pool.doJob(bind(silly, rand() % 5));
        // pool.doJob(bind(silly, 2));
        // sleep(5);
    }
}