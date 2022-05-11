#ifndef MY_MUDUO_NET_EVENTLOOP_THREAD_H
#define MY_MUDUO_NET_EVENTLOOP_THREAD_H

#include "Condition.h"
#include "Thread.h"

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
            Thread thread_;
            MutexLock mutex_;
            Condition cond_ GUARDED_BY(mutex_);
            EventLoop *loop_ GUARDED_BY(mutex_);
            bool exiting_;
            const ThreadInitCallback &callback_;
        };
    }
}

#endif // !MY_MUDUO_NET_EVENTLOOP_THREAD_H