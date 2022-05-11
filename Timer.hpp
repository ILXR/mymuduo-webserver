#ifndef MY_TIMER_HPP
#define MY_TIMER_HPP

#include "noncopyable.h"
#include "Timestamp.h"
#include "Callbacks.h"
#include "Atomic.h"

namespace mymuduo
{
    namespace net
    {

        class Timer : noncopyable
        {
        private:
            const TimerCallback callback_;
            Timestamp expiration_;
            const double interval_;
            const bool repeat_;

            /**
             * 只用指针，无法区分地址相同的先后两个Timer对象
             * 因此每个Timer对象有一个全局递增的序列号int64_t sequence_(用原子计数器(AtomicInt64)生成)
             * TimerId同时保存Timer*和 sequence_
             * 这样TimerQueue::cancel()就能根据TimerId找到需要注销的 Timer对象
             */
            const int64_t sequence_;
            static AtomicInt64 s_numCreated_;

        public:
            Timer(const TimerCallback &cb, Timestamp when, double interval)
                : callback_(cb), expiration_(when), interval_(interval),
                  repeat_(interval > 0), sequence_(s_numCreated_.incrementAndGet()){};

            Timestamp expiration() { return expiration_; }
            bool repeat() const { return repeat_; }
            int64_t sequence() const { return sequence_; }

            static int64_t numCreated() { return s_numCreated_.get(); }

            void run() const { callback_(); }

            void restart(Timestamp now)
            {
                if (repeat_)
                {
                    expiration_ = addTime(now, interval_);
                }
                else
                {
                    expiration_ = Timestamp::invalid();
                }
            }
        };

        AtomicInt64 Timer::s_numCreated_;
    }
}

#endif // !MY_TIMER_HPP