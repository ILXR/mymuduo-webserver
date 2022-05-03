#include "EventLoop.h"
#include "util.h"
#include <cstdio>
#include <cassert>
#include <poll.h>
#include <stdio.h>

namespace mymuduo
{

    __thread EventLoop *t_loopInThisThread = 0;

    EventLoop::EventLoop()
        : looping_(false),
          threadId_(muduo::CurrentThread::tid())
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

    void EventLoop::loop()
    {
        // 事件循环必须在IO线程执行，因此EventLoop::loop()会检查这一 pre-condition
        assert(!looping_);
        assertInLoopThread();
        looping_ = true;

        // 什么都不做，等5秒就退出
        ::poll(NULL, 0, 5 * 1000);

        printf("EventLoop stop looping\n");
        looping_ = false;
    }
    void EventLoop::abortNotInLoopThread()
    {
        printf("EventLoop::abortNotInLoopThread - EventLoop was created in threadId_ = \
%d , but current thread id = %d\n",
               threadId_, muduo::CurrentThread::tid());
    }
}