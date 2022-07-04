/**
 * 有界队列，也是生产者消费者队列，用一个 mutex 两个 condition
 * 需要封装queue的pop和push操作，还有size、empty
 */

#ifndef MYMUDUO_BOUNDEDBLOCKINGQUEUE_H
#define MYMUDUO_BOUNDEDBLOCKINGQUEUE_H

#include "mymuduo/base/Condition.h"
#include "mymuduo/base/Mutex.h"

#include <deque>
#include <boost/circular_buffer.hpp>

namespace mymuduo
{
    /**
     * 线程安全的有界生产者/消费者队列
     * 使用一个 mutex 和两个 condition 实现
     */
    template <typename T>
    class BounedeBlockingQueue
    {
    public:
        // 使用了 boost 的环形缓冲区，size 是固定的
        using queue_type = boost::circular_buffer<T>;
        BounedeBlockingQueue(int maxSize)
            : mutex_(), notFull_(mutex_), notEmpty_(mutex_), queue_(maxSize)
        {
        }

        void put(T &val)
        {
            MutexLockGuard lock(mutex_);
            while (queue_.full())
            {
                notFull_.wait();
            }
            assert(!queue_.full());
            queue_.push_back(val);
            notEmpty_.notify();
        }

        void put(T &&val)
        {
            MutexLockGuard lock(mutex_);
            while (queue_.full())
            {
                notFull_.wait();
            }
            assert(!queue_.full());
            queue_.push_back(std::move(val));
            notEmpty_.notify();
        }

        T take()
        {
            MutexLockGuard lock(mutex_);
            while (queue_.empty())
            {
                notEmpty_.wait();
            }
            assert(!queue_.empty());
            T front(std::move(queue_.front()));
            queue_.pop_front();
            notFull_.notify();
            return front;
        }

        bool full()
        {
            MutexLockGuard lock(mutex_);
            return queue_.full();
        }

        bool empty()
        {
            MutexLockGuard lock(mutex_);
            return queue_.empty();
        }

        size_t size()
        {
            MutexLockGuard lock(mutex_);
            return queue_.size();
        }

        size_t capacity()
        {
            MutexLockGuard lock(mutex_);
            return queue_.capacity();
        }

    private:
        MutexLock mutex_;
        Condition notFull_ GUARDED_BY(mutex_);
        Condition notEmpty_ GUARDED_BY(mutex_);
        queue_type queue_ GUARDED_BY(mutex_);
    };
}

#endif