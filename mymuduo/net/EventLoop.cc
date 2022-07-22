#include "mymuduo/net/EventLoop.h"

#include "mymuduo/base/Mutex.h"
#include "mymuduo/base/Logging.h"
#include "mymuduo/net/Poller.h"
#include "mymuduo/net/Channel.h"
#include "mymuduo/net/TimerQueue.h"
#include "mymuduo/net/SocketsOps.h"

#include <algorithm>
#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace mymuduo;
using namespace mymuduo::net;

namespace
{
    // 每个线程仅有一个实例，是线程全局变量
    __thread EventLoop *t_loopInThisThread = 0;

    const int kPollTimeMs = 10000; // poll阻塞时间，可以修改

    int createEventfd()
    {
        int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (evtfd < 0)
        {
            // LOG_SYSERR << "Failed in eventfd";
            perror("Failed in eventfd\n");
            abort();
        }
        return evtfd;
    }

#pragma GCC diagnostic ignored "-Wold-style-cast" // 旧式强制转换
    /**
     * 默认情况下，往一个读端关闭的管道或socket连接中写数据将引发SIGPIPE信号
     * 对一个已经收到FIN包的socket调用read方法, 如果接收缓冲已空, 则返回0, 这就是常说的表示连接关闭.
     * 但第一次对其调用write方法时, 如果发送缓冲没问题, 会返回正确写入(发送).
     * 但发送的报文会导致对端发送RST报文, 因为对端的socket已经调用了close, 完全关闭,
     * 既不发送, 也不接收数据.
     * 所以, 第二次调用write方法(假设在收到RST之后), 会生成SIGPIPE信号, 导致进程退出
     * 但是 IO 复用已经提供了相关检查功能：
     * 以poll为例，当管道的读端关闭时，写端文件描述符上的POLLHUP事件将被触发；当socket连接被对方关闭时，socket上的POLLRDHUP事件将被触发。
     */
    class IgnoreSigPipe
    {
    public:
        IgnoreSigPipe()
        {
            ::signal(SIGPIPE, SIG_IGN);
        }
    };
#pragma GCC diagnostic error "-Wold-style-cast"

    IgnoreSigPipe initObg;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      timerQueue_(new TimerQueue(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG << "EventLoop created " << this << " in Thread" << threadId_;
    // 一个线程一个 EventLoop，所以不存在线程安全的问题
    if (t_loopInThisThread)
    {
        LOG_FATAL << "Another EventLoop " << t_loopInThisThread << " has existed in thread " << threadId_;
    }
    else
    {
        t_loopInThisThread = this;
    }
    wakeupChannel_->setReadCallback(
        std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    assert(!looping_);
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = NULL;
}

EventLoop *EventLoop::getEventLoopOfCurrentThread()
{
    return t_loopInThisThread;
}

/**
 * 调用Poller::poll()获得当 前活动事件的Channel列表，然后依次调用每个Channel的handleEvent() 函数
 */
void EventLoop::loop()
{
    // 事件循环必须在IO线程执行，因此EventLoop::loop()会检查这一 pre-condition
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    while (!quit_)
    {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            channel->handleEvent(pollReturnTime_);
        }
        doPendingFunctors();
    }
    LOG_DEBUG << "EventLoop " << this << " stop looping";
    looping_ = false;
}

/**
 * 检查断言之后调用 Poller::updateChannel()，EventLoop不关心Poller是如何管理Channel列表的
 */
void EventLoop::updateChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

/**
 * 支持 unregister 功能
 * （因为 TcpConnection 不再自生自灭了，可以进行管理）
 */
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
    LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop was created in threadId_ = "
              << threadId_ << ", but current thread id = " << CurrentThread::tid();
}

TimerId EventLoop::runAt(const Timestamp &time, const TimerCallback &cb)
{
    return timerQueue_->addTimer(cb, time, 0.0);
}
TimerId EventLoop::runAfter(double delay, const TimerCallback &cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, cb);
}
TimerId EventLoop::runEvery(double interval, const TimerCallback &cb)
{
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(cb, time, interval);
}
void EventLoop::cancel(TimerId timerId)
{
    timerQueue_->cancel(timerId);
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        cb();
    }
    else
    {
        // 不在创建的线程上，先加入队列
        queueInLoop(std::move(cb));
    }
}

/**
 * 在它的IO线程内执行某个用户 任务回调，即EventLoop::runInLoop(const Functor& cb)
 * 其中Functor是 std::function<void()>。如果用户在当前IO线程调用这个函数,
 * 回调会同步进行；如果用户在其他线程调用runInLoop()，cb会被加入队列，
 * IO 线程会被唤醒来调用这个Functor
 */
void EventLoop::queueInLoop(Functor cb)
{
    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.push_back(cb);
    }
    // 如果调用queueInLoop()的线程不是IO线程， 那么唤醒是必需的；
    // 如果在IO线程调用queueInLoop()，而此时正在调用 pending functor，那么也必须唤醒
    // 否则下一轮的poll会监听其他的fd，而这个新添加的cb只有在其他fd监听到事件时才会执行
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

/**
 * EventLoop::doPendingFunctors()不是简单地在临界区内依次调用 Functor，
 * 而是把回调列表swap()到局部变量functors中，这样一方面减小了临界区的长度
 * （意味着不会阻塞其他线程调用queueInLoop()），
 * 另一方面也避免了死锁（因为Functor可能再调用queueInLoop()）
 */
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        /**
         * 由于doPendingFunctors()调用的Functor可能再调用 queueInLoop(cb)，
         * 这时queueInLoop()就必须wakeup()，否则这些新加的 cb就不能被及时调用了
         */
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    for (size_t i = 0; i < functors.size(); ++i)
    {
        functors[i]();
    }
    callingPendingFunctors_ = false;
}

void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_DEBUG << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_DEBUG << "EventLoop::wakeup() reads " << n << " bytes instead of 8";
    }
}

void EventLoop::quit()
{
    // 在必要时唤醒IO线程，让它及时终止循环
    quit_ = true;
    if (!isInLoopThread())
    {
        wakeup();
    }
}
