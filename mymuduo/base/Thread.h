#ifndef MY_MUDUO_BASE_THREAD_H
#define MY_MUDUO_BASE_THREAD_H

#include "mymuduo/base/CountDownLatch.h"
#include "mymuduo/base/noncopyable.h"
#include "mymuduo/base/Atomic.h"
#include "mymuduo/base/Types.h"

#include <functional>
#include <memory>
#include <pthread.h>

namespace mymuduo
{

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
        // Thread既有pthread_t也有pid_t，它们各有用处，pthread_t给pthread_XXX函数使用，而pid_t作为线程标识。
        pthread_t pthreadId_;
        /**
         * pthread_t的值很大，无法作为一些容器的key值。 glibc的Pthreads实现实际上把pthread_t作为一个结构体指针，
         * 指向一块动态分配的内存，但是这块内存是可以反复使用的，也就是说很容易造成pthread_t的重复。
         * 也就是说 pthreads 只能保证同一进程内，同一时刻的各个线程不同；不能保证同一个进程全程时段每个线程具有不同的id，
         * 不能保证线程id的唯一性。
         * 所以要用 tid 来识别内核线程：linux分配新的pid采用递增轮回办法，短时间内启动多个线程也会具有不同的id。
         */
        pid_t tid_;
        ThreadFunc func_;
        string name_;
        CountDownLatch latch_;

        static AtomicInt32 numCreated_;
    };

} // namespace muduo
#endif // MUDUO_BASE_THREAD_H
