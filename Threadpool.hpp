#ifndef THREADPOOL_HPP_
#define THREADPOOL_HPP_ 

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

class ThreadPool {
    // from https://stackoverflow.com/questions/26516683/reusing-thread-in-loop-c
    public:

    ThreadPool(int threads);

    ~ThreadPool(); 

    void doJob(std::function<void(void)> func); 

    protected:

    void threadEntry(int i); 

    std::mutex lock_;
    std::condition_variable condVar_;
    bool shutdown_;
    std::queue<std::function<void(void)>> jobs_;
    std::vector<std::thread> threads_;
};

#endif