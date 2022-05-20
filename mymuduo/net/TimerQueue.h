#ifndef MY_TUMERQUEUE_H
#define MY_TUMERQUEUE_H

#include "mymuduo/base/Timestamp.h"
#include "mymuduo/net/Callbacks.h"
#include "mymuduo/net/Channel.h"
#include "mymuduo/base/Mutex.h"

#include <vector>
#include <set>

namespace mymuduo
{
    namespace net
    {

        class EventLoop;
        class TimerId;
        class Timer;
        /**
         * muduo的定时器功能由三个class实现，TimerId、Timer、 TimerQueue
         * 用户只能看到第一个class，另外两个都是内部实现细节
         */
        class TimerQueue
        {
        private:
            // 以pair<Timestamp, Timer*>为key，这样即便两个Timer的到期时间相同，它们的地址也必定不同
            typedef std::pair<Timestamp, Timer *> Entry; // Timer 用裸指针管理，这里是可以优化的地方（考虑 unique_ptr）
            typedef std::set<Entry> TimerList;
            typedef std::pair<Timer *, int64_t> ActiveTimer;
            typedef std::set<ActiveTimer> ActiveTimerSet;

            EventLoop *loop_;
            const int timerfd_;
            Channel timerfdChannel_;

            // for cancel
            bool callingExpiredTimers_;
            TimerList timers_; // 按到期时间排列
            /**
             * activeTimers_保存的是目前有效的 Timer的指针，并满足invariant：
             *      timers_.size() == activeTimers_.size()
             * 因为这两个容器保存的是相同的数据,
             * 只不过timers_是按到期时间排序, activeTimers_是按对象地址排序(利用了set对pair的排序机制)
             */
            ActiveTimerSet activeTimers_;
            ActiveTimerSet cancelingTimers_;

            void addTimerInLoop(Timer *timer);
            void cancelInLoop(TimerId timerId);

            // called when timerfd alarms
            void handleRead();
            // 移动所有到期的timer
            std::vector<Entry> getExpired(Timestamp now);

            void reset(const std::vector<Entry> &expired, Timestamp now);
            bool insert(Timer *timer);

        public:
            explicit TimerQueue(EventLoop *loop);
            ~TimerQueue();

            TimerId addTimer(TimerCallback cb, Timestamp when, double interval);
            /* cancel()有对应的cancelInLoop()函数，因此TimerQueue不必用锁。 */
            void cancel(TimerId timerId);
        };
    }
}

#endif // !MY_TUMERQUEUE_H