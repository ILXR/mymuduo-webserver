#ifndef MY_EVENTLOOPTHREAD_H
#define MY_EVENTLOOPTHREAD_H

#include <muduo/base/Thread.h>
#include <muduo/base/Mutex.h>

using namespace std;
namespace mymuduo
{

    class EventLoop;
    class ThreadInitCallback;

    class EventLoopThread
    {
    public:
        typedef std::function<void(EventLoop *)> ThreadInitCallback;
        EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                        const string &name = string());
        ~EventLoopThread();

        EventLoop *startLoop();
        void threadFunc();

    private:
        muduo::Thread thread_;
        muduo::MutexLock mutex_;
        muduo::Condition cond_;
        EventLoop *loop_;
        bool exiting_;
        const ThreadInitCallback &callback_;
    };

}

#endif // !MY_EVENTLOOPTHREAD_H