#ifndef MY_MUDUO_CONDITION_H
#define MY_MUDUO_CONDITION_H

#include "Mutex.h"
#include <pthread.h>

namespace mymuduo
{

    class Condition : noncopyable
    {
    public:
        explicit Condition(MutexLock &mutex) : mutex_(mutex) { MCHECK(pthread_cond_init(&pcond_, NULL)); }
        ~Condition() { MCHECK(pthread_cond_destroy(&pcond_)); }

        void wait()
        {
            MutexLock::UnassignGuard ug(mutex_);
            MCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()));
        }

        // 超时返回 true
        bool waitForSeconds(double seconds);

        /**
         * 一次只能唤醒一个线程，适用于：
         *      * 单一生产者，生产者一次生产一个产品的情况，最好一个消费者
         */
        void notify() { MCHECK(pthread_cond_signal(&pcond_)); }
        /**
         * 能同时唤醒多个在等待的线程，适用于：
         *      * 一个生产者多消费者，生产者能一次产生多个产品的情况。
         *      * 多生产者多消费者
         *      * 读写锁实现（写入之后，通知所有读者）
         */
        void notifyAll() { MCHECK(pthread_cond_broadcast(&pcond_)); }

    private:
        // posix1标准说，pthread_cond_signal与pthread_cond_broadcast无需考虑调用线程是不是mutex的拥有者
        MutexLock &mutex_;
        pthread_cond_t pcond_;
    };
}

#endif // !MY_MUDUO_CONDITION_H