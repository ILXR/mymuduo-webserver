#include "EventLoop.h"
#include "Channel.h"
#include "Poller.h"
#include "TimerId.h"
#include "TimerQueue.h"
#include <muduo/net/Callbacks.h>
#include <muduo/base/Timestamp.h>
#include <cstdio>
#include <cassert>
#include <poll.h>
#include <stdio.h>

namespace mymuduo
{

    using TimerCallback = muduo::net::TimerCallback;

    __thread EventLoop *t_loopInThisThread = 0;

    EventLoop::EventLoop()
        : looping_(false),
          quit_(false),
          threadId_(muduo::CurrentThread::tid()),
          poller_(new Poller(this)),
          timerQueue_(new mymuduo::TimerQueue(this))
    {
        printf("EventLoop created in %d\n", threadId_);
        // 一个线程一个 EventLoop，所以不存在线程安全的问题
        if (t_loopInThisThread)
        {
            printf("Another EventLoop has exists in thread %d\n", threadId_);
            exit(1);
        }
        else
        {
            t_loopInThisThread = this;
        }
    }

    EventLoop::~EventLoop()
    {
        assert(!looping_);
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
            poller_->poll(kPollTimeMs_, &activeChannels_);
            for (ChannelList::iterator it = activeChannels_.begin(); it != activeChannels_.end(); ++it)
            {
                (*it)->handleEvent();
            }
        }

        printf("EventLoop stop looping\n");
        looping_ = false;
    }

    /**
     * 检查断言之后调用 Poller::updateChannel()，EventLoop不关心Poller是如何管理Channel列表 的
     */
    void EventLoop::updateChannel(Channel *channel)
    {
        assert(channel->ownerLoop() == this);
        assertInLoopThread();
        poller_->updateChannel(channel);
    }

    void EventLoop::abortNotInLoopThread()
    {
        printf("EventLoop::abortNotInLoopThread - EventLoop was created in threadId_ = \
%d , but current thread id = %d\n",
               threadId_, muduo::CurrentThread::tid());
    }

    TimerId EventLoop::runAt(const Timestamp &time, const TimerCallback &cb)
    {
        return timerQueue_->addTimer(cb, time, 0.0);
    }
    TimerId EventLoop::runAfter(double delay, const TimerCallback &cb)
    {
        Timestamp time(muduo::addTime(Timestamp::now(), delay));
        return runAt(time, cb);
    }
    TimerId EventLoop::runEvery(double interval, const TimerCallback &cb)
    {
        Timestamp time(muduo::addTime(Timestamp::now(), interval));
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
            // printf("add to word queue\n");
            // cb();
            // 不在创建的线程上，先加入队列
            // queueInLoop(std::move(cb));
        }
    }
}