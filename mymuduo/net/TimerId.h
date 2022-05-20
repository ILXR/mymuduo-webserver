#ifndef MY_MUDUO_NET_TIMERID_H
#define MY_MUDUO_NET_TIMERID_H

#include "mymuduo/base/copyable.h"
#include <cstddef>
#include <cstdint>

namespace mymuduo
{
    namespace net
    {

        class Timer;
        class TimerQueue;

        class TimerId : public copyable
        {
        public:
            TimerId()
                : timer_(NULL),
                  sequence_(0)
            {
            }

            TimerId(Timer *timer, int64_t seq)
                : timer_(timer),
                  sequence_(seq)
            {
            }

            friend class TimerQueue;

        private:
            Timer *timer_;
            int64_t sequence_;
        };
    }
}

#endif // MY_MUDUO_NET_TIMERID_H