#ifndef MY_EVENT_LOOP_H
#define MY_EVENT_LOOP_H

#include <pthread.h>
#include <muduo/base/CurrentThread.h>
#include "noncopyable.h"
// #include "CurrentThread.h"

namespace mymuduo
{

    class Channel;
    class EventLoop : noncopyable
    {
    private:
        void abortNotInLoopThread();
        bool looping_;
        const pid_t threadId_;

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

        void updateChannel(Channel *channel);

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