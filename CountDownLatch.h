#ifndef MY_MUDUO_COUNTDOWNLATCH_H
#define MY_MUDUO_COUNTDOWNLATCH_H

#include "Condition.h"

namespace mymuduo
{

    /**
     * 计数器，使用 pthread_condition 来等待计数变为0
     * 变为0后通过 condition nodifyall 来通知所有等待的线程
     */
    class CountDownLatch
    {
    public:
        explicit CountDownLatch(int count);
        ~CountDownLatch(){};

        void wait();
        void countDown();
        int getCount() const;

    private:
        mutable MutexLock mutex_;
        Condition condition_;
        int count_;
    };

}

#endif // !MY_MUDUO_COUNTDOWNLATCH_H