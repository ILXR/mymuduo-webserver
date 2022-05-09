#ifndef MYMUDUO_BASE_THREAD_H
#define MYMUDUO_BASE_THREAD_H

#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Atomic.h"
#include "noncopyable.h"
#include "Types.h"

#include <functional>
#include <memory>
#include <pthread.h>

namespace mymuduo
{
    using CountDownLatch = muduo::CountDownLatch;
    using AtomicInt32 = muduo::AtomicInt32;

    class Thread : noncopyable
    {
    public:
        typedef std::function<void()> ThreadFunc;

        explicit Thread(ThreadFunc, const string &name = string());
        // FIXME: make it movable in C++11
        ~Thread();

        void start();
        int join(); // return pthread_join()

        bool started() const { return started_; }
        // pthread_t pthreadId() const { return pthreadId_; }
        pid_t tid() const { return tid_; }
        const string &name() const { return name_; }

        static int numCreated() { return numCreated_.get(); }

    private:
        void setDefaultName();

        bool started_;
        bool joined_;
        pthread_t pthreadId_;
        pid_t tid_;
        ThreadFunc func_;
        string name_;
        CountDownLatch latch_;

        static AtomicInt32 numCreated_;
    };

} // namespace muduo
#endif // MUDUO_BASE_THREAD_H
