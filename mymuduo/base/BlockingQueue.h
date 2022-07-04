#ifndef MYMUDUO_BLOCKINGQUEUE_H
#define MYMUDUO_BLOCKINGQUEUE_H

#include "mymuduo/base/Condition.h"
#include "mymuduo/base/Mutex.h"

#include <deque>
#include <assert.h>

namespace mymuduo
{
    /**
     * 普通的线程安全队列
     * 使用一个 mutex 和一个 condition 实现
     */
    template <typename T>
    class BlockingQueue : noncopyable
    {
    public:
        using queue_type = std::deque<T>;

        BlockingQueue() : mutex_(), notEmpty_(mutex_), queue_()
        {
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
            return front;
        }

        void put(T &val)
        {
            MutexLockGuard lock(mutex_);
            queue_.push_back(val);
            // 因为每次只能取一个值，因此使用 notify 而不是 notifyall
            notEmpty_.notify();
        }

        void put(T &&val)
        {
            MutexLockGuard lock(mutex_);
            queue_.emplace_back(val);
            notEmpty_.notify();
        }

        size_t size() const
        {
            MutexLockGuard lock(mutex_);
            return queue_.size();
        }

        /**
         *  @brief  清空队列并返回其原始数据
         */
        queue_type drain()
        {
            std::deque<T> queue;
            {
                MutexLockGuard lock(mutex_);
                queue = std::move(queue_);
                assert(queue_.empty());
            }
            return queue;
        }

    private:
        mutable MutexLock mutex_; // 被mutable修饰的变量，将永远处于可变的状态，即使在一个const函数中
        Condition notEmpty_ GUARDED_BY(mutex_);
        queue_type queue_ GUARDED_BY(mutex_);
    };
}

#endif