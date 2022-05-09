#ifndef MY_EVENTLOOPTHREAD_H
#define MY_EVENTLOOPTHREAD_H

#include "Thread.h"
#include <muduo/base/Mutex.h>

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
            muduo::MutexLock mutex_;
            muduo::Condition cond_ GUARDED_BY(mutex_);
            EventLoop *loop_ GUARDED_BY(mutex_);
            bool exiting_;
            const ThreadInitCallback &callback_;
        };
    }
}

#endif // !MY_EVENTLOOPTHREAD_H