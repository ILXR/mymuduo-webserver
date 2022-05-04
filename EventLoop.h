#ifndef MY_EVENT_LOOP_H
#define MY_EVENT_LOOP_H

#include <functional>
#include <pthread.h>
#include <vector>
#include <muduo/base/CurrentThread.h>
#include <boost/scoped_ptr.hpp>
#include <muduo/net/Callbacks.h>
#include <muduo/base/Timestamp.h>
#include "noncopyable.h"
#include "TimerId.h"

namespace mymuduo
{

    class Channel;
    class Poller;
    class TimerQueue;
    using Timestamp = muduo::Timestamp;
    using TimerCallback = muduo::net::TimerCallback;
    class EventLoop : noncopyable
    {
    private:
        void abortNotInLoopThread();

        typedef std::function<void()> Functor;
        typedef std::vector<Channel *> ChannelList;

        bool looping_;
        bool quit_;
        const pid_t threadId_;
        int kPollTimeMs_ = 1000; // poll阻塞时间，可以修改

        // 注意EventLoop通过scoped_ptr来间接持有 Poller
        boost::scoped_ptr<Poller> poller_;
        ChannelList activeChannels_;
        mymuduo::TimerQueue *timerQueue_;

    public:
        /*
         * 每个线程只能有一个EventLoop对象，
         * 因此EventLoop的构造函数会检查当前线程是否已经创建了其他EventLoop 对象
         * 遇到错误就终止程序（LOG_FATAL)
         **/
        EventLoop();
        ~EventLoop();

        /*
         * 创建了EventLoop对象的线程是 IO线程，其主要功能是运行事件循环EventLoop:: loop()
         * EventLoop对象 的生命期通常和其所属的线程一样长，它不必是heap对象
         **/
        void loop();
        void quit() { quit_ = true; };
        void updateChannel(Channel *channel);

        void runInLoop(Functor cb);
        void cancel(TimerId timerId);
        // Timer Event
        TimerId runAt(const Timestamp &time, const TimerCallback &cb);
        TimerId runAfter(double delay, const TimerCallback &cb);
        TimerId runEvery(double interval, const TimerCallback &cb);

        /*
         * muduo的接口设计会明确哪些成员函数是线程安全的，可以跨线程调用；
         * 哪些成员函数只能在某个特定线程调用（主要是IO线程）。
         * 为了能在运行时检查这些pre-condition，EventLoop提供了
         * isInLoopThread()和 assertInLoopThread()等函数
         */
        bool isInLoopThread() const { return threadId_ == muduo::CurrentThread::tid(); }

        /* 只能在所属线程执行，这里进行检查 */
        void assertInLoopThread()
        {
            if (!isInLoopThread())
            {
                abortNotInLoopThread();
            }
        }

        /*
         * 返回该线程对应的唯一 EventLoop
         */
        static EventLoop *getEventLoopOfCurrentThread();
    };
}

#endif // !MY_EVENT_LOOP_H