#include "mymuduo/net/TimerQueue.h"

#include "mymuduo/base/Logging.h"
#include "mymuduo/net/EventLoop.h"
#include "mymuduo/net/Callbacks.h"
#include "mymuduo/net/TimerId.h"
#include "mymuduo/net/Timer.hpp"

#include <functional>
#include <sys/timerfd.h>

namespace mymuduo
{
    namespace detail
    {
        /**
         *  @brief  创建一个定时器描述符，用于TimerQueue
         */
        int createTimerfd()
        {
            // timerf_create(2)函数把时间变成了一个描述符，该文件描述符在定时器超时的那一刻变得可读
            // 这样就方便融入select(2)或poll(2)中去
            /**
             *  @brief  int timerfd_create(int clockid, int flags);
             *  @param  clockid 参数指定时钟，用于标志timer的进度，它必须是下面的一个:
             *          CLOCK_REALTIME  系统实时时间,随系统实时时间改变而改变,即从 UTC 1970-1-1 0:0:0 开始计时。
             *          CLOCK_MONOTONIC 从系统启动这一刻起开始计时,不受系统时间被用户改变的影响
             *  @param  flags
             *          TFD_NONBLOCK   ： 非阻塞模式
             *          TFD_CLOEXEC    ： 当程序执行exec函数时本fd将被系统自动关闭,表示不传递
             */
            int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
            if (timerfd < 0)
            {
                LOG_SYSFATAL << "Failed in timerfd_create";
            }
            return timerfd;
        }

        /**
         * 计算 Timestamp 从创建到现在的时间
         */
        struct timespec howMuchTimeFromNow(Timestamp when)
        {
            int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
            if (microseconds < 100)
            {
                microseconds = 100;
            }
            struct timespec ts;
            ts.tv_sec = static_cast<time_t>(
                microseconds / Timestamp::kMicroSecondsPerSecond);
            ts.tv_nsec = static_cast<long>(
                (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
            return ts;
        }

        /**
         *  @brief  触发 TimerFd 可读后需要read来清除状态，否则会一直触发(poll 是 level 的)
         */
        void readTimerfd(int timerfd, Timestamp now)
        {
            uint64_t howmany;
            ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
            LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
            if (n != sizeof howmany)
            {
                // 写入 u64 所以读取也需要是对应大小才正确
                LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
            }
        }

        void resetTimerfd(int timerfd, Timestamp expiration)
        {
            // wake up loop by timerfd_settime()
            struct itimerspec newValue;
            struct itimerspec oldValue;
            memset(&newValue, 0, sizeof newValue);
            memset(&oldValue, 0, sizeof oldValue);
            newValue.it_value = howMuchTimeFromNow(expiration);
            int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
            if (ret)
            {
                LOG_SYSERR << "timerfd_settime()";
            }
        }
    }

}
using namespace mymuduo;
using namespace mymuduo::net;
using namespace mymuduo::detail;

TimerQueue::TimerQueue(EventLoop *loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      callingExpiredTimers_(false),
      timers_()
{
    // 绑定回调函数, 开启监听
    timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
}

void TimerQueue::cancel(TimerId timerId)
{
    // 例用EventLoop::runInLoop()将调用转发 到IO线程
    loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(timer);
    // 由于TimerId不负责Timer的生命期，其中保存的Timer*可能失效
    // 因此不能直接dereference，只有在activeTimers_中找到了Timer时才能提领
    if (it != activeTimers_.end())
    {
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first)); // 返回删除的个数
        assert(n == 1);
        (void)n;
        delete it->first; // 释放Timer对象
        activeTimers_.erase(it);
    }
    else if (callingExpiredTimers_)
    {
        // 为了应对“自注销”的情况
        cancelingTimers_.insert(timer);
    }
    assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
    printf("handle\n");
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);

    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    // safe to callback outside critical section
    for (const Entry &it : expired)
    {
        it.second->run();
    }
    callingExpiredTimers_ = false;

    reset(expired, now);
}

/**
 * 从timers_中移除已到期的Timer，并通过vector返回它们。
 * 编译器会实施RVO优化，不必太担心性能，必要时可以像EventLoop::activeChannels_那样复用vector。
 * 注意其中哨兵值（sentry）的选取，sentry让set::lower_bound()返回的是第一个未到期的Timer的迭代器，
 * 因此断言中是<而非≤
 */
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now) // expired - (因到期而)失效，终止
{
    std::vector<Entry> expired;
    Entry sentry = std::make_pair(now, reinterpret_cast<Timer *>(UINTPTR_MAX));
    TimerList::iterator iter = timers_.lower_bound(sentry);
    assert(iter == timers_.end() || now < iter->first);
    std::copy(timers_.begin(), iter, back_inserter(expired));
    timers_.erase(timers_.begin(), iter);

    for (const Entry &it : expired)
    {
        ActiveTimer timer(it.second, it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1);
        /**
         * 把gcc的编译严格等级设置为任何警告当成错误，而有的变量实际上是在debug版本中assert时有用的
         * release时这个变量就没用了， 此时编译器就会提醒有个变量没用的警告
         * 这时就会被当做错误了。所以用(void)n 避免这个错误
         */
        (void)n;
    }

    return expired;
}

/**
 * 调用辅助函数 addTimerInLoop ，将其作为任务添加到 EventLoop 任务队列中（需保证在统一线程运行）
 */
TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
{
    Timer *timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}

void TimerQueue::addTimerInLoop(Timer *timer)
{
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);
    if (earliestChanged)
    {
        resetTimerfd(timerfd_, timer->expiration());
    }
}

void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now)
{
    Timestamp nextExpire; // 每次将所有定时器中最近要过期的时间作为触发时间，触发后再寻找下一过期时间
    for (const Entry &it : expired)
    {
        ActiveTimer timer(it.second, it.second->sequence());
        if (it.second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end())
        {
            // 处理周期定时器
            it.second->restart(now);
            // 将新的周期定时器插入两个set中
            insert(it.second);
        }
        else
        {
            // FIXME move to a free list
            delete it.second; // FIXME: no delete please
        }
    }

    if (!timers_.empty())
    {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid())
    {
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(Timer *timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first) // 如果队列为空或者timer时间比最近过期的还要早
    {
        earliestChanged = true;
    }
    {
        std::pair<TimerList::iterator, bool> result = timers_.insert(Entry(when, timer));
        assert(result.second);
        (void)result;
    }
    {
        std::pair<ActiveTimerSet::iterator, bool> result = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second);
        (void)result;
    }

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}