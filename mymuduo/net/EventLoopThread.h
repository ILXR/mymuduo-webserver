#ifndef MY_MUDUO_NET_EVENTLOOP_THREAD_H
#define MY_MUDUO_NET_EVENTLOOP_THREAD_H

#include "mymuduo/base/Condition.h"
#include "mymuduo/base/Thread.h"

using namespace std;
namespace mymuduo
{
    namespace net
    {

        class EventLoop;

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
            EventLoop *loop_ GUARDED_BY(mutex_);
            Thread thread_;
            MutexLock mutex_;
            Condition cond_ GUARDED_BY(mutex_);
            const ThreadInitCallback &callback_;
            bool exiting_;
        };
    }
}

#endif // !MY_MUDUO_NET_EVENTLOOP_THREAD_H