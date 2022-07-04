#ifndef MYMUDUO_BASE_THREADPOOL_H
#define MYMUDUO_BASE_THREADPOOL_H

#include "mymuduo/base/Condition.h"
#include "mymuduo/base/Mutex.h"
#include "mymuduo/base/Thread.h"
#include "mymuduo/base/Types.h"

#include <deque>
#include <vector>

namespace mymuduo
{
    class ThreadPool : noncopyable
    {
    public:
        typedef std::function<void()> Task;

        explicit ThreadPool(const string &nameArg = string("ThreadPool"));
        ~ThreadPool();

        // Must be called before start().
        void setMaxQueueSize(int maxSize) { maxQueueSize_ = maxSize; }
        void setThreadInitCallback(const Task &cb) { threadInitCallback_ = cb; }

        void start(int numThreads);
        void stop();

        const string &name() const
        {
            return name_;
        }

        size_t queueSize() const;

        // Could block if maxQueueSize > 0
        // Call after stop() will return immediately.
        // There is no move-only version of std::function in C++ as of C++14.
        // So we don't need to overload a const& and an && versions
        // as we do in (Bounded)BlockingQueue.
        // https://stackoverflow.com/a/25408989
        void run(Task f);

    private:
        bool isFull() const REQUIRES(mutex_);
        void runInThread();
        Task take();

        mutable MutexLock mutex_;
        Condition notEmpty_ GUARDED_BY(mutex_);
        Condition notFull_ GUARDED_BY(mutex_);
        string name_;
        Task threadInitCallback_; // 每个线程被创建后，开始工作前都会调用这个回调函数
        std::vector<std::unique_ptr<mymuduo::Thread>> threads_;
        // 由于BoundedBlockingQueue类需要在初始化时明确队列任务的上限，而线程池实际是在运行时确定、调整，
        // 不能控制任务队列从在入队、出队的阻塞状态退出，导致线程池在调用关闭时会阻塞，因此没有将其作为成员变量。
        std::deque<Task> queue_ GUARDED_BY(mutex_);
        // 队列限制了任务的总数，在网络通信中是达到连接上限作出拒绝访问的处理。
        size_t maxQueueSize_;
        bool running_;
    };
}

#endif