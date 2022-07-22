#include "mymuduo/net/EventLoopThread.h"

#include "mymuduo/net/EventLoop.h"

#include <assert.h>

using namespace mymuduo;
using namespace mymuduo::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const string &name)
    : loop_(NULL),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(mutex_),
      callback_(cb),
      exiting_(false)
{
}
EventLoopThread::~EventLoopThread()
{
    if (loop_ != NULL)
    {
        loop_->quit();
        thread_.join();
    }
    exiting_ = true;
}

/**
 * 返回新线程中EventLoop对象的地址，因此用条件变量来等待线程的创建与运行
 */
EventLoop *EventLoopThread::startLoop()
{
    assert(!thread_.started());

    thread_.start();
    {
        MutexLockGuard lock(mutex_);
        while (loop_ == NULL)
        {
            cond_.wait();
        }
    }
    return loop_;
}

/**
 * 线程主函数在stack上定义EventLoop对象，然后将其地址赋值给 loop_成员变量
 * 最后notify()条件变量，唤醒startLoop()。
 * 也就是说这个 loop 是在新线程上创建的唯一 loop
 * 由于EventLoop的生命期与线程主函数的作用域相同
 * 因此在 threadFunc()退出之后这个指针就失效了
 * 等同于 loop 的生命周期和 Thread 相同
 */
void EventLoopThread::threadFunc()
{
    EventLoop loop;
    if (callback_)
    {
        callback_(&loop);
    }
    {
        MutexLockGuard lock(mutex_);
        loop_ = &loop;
        cond_.notify();
    }
    loop.loop();
}
