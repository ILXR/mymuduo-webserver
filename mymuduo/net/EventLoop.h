#ifndef MY_MUDUO_NET_EVENTLOOP_H
#define MY_MUDUO_NET_EVENTLOOP_H

#include <vector>
#include <functional>
#include <boost/scoped_ptr.hpp>

#include "mymuduo/base/Mutex.h"
#include "mymuduo/base/Timestamp.h"
#include "mymuduo/base/CurrentThread.h"
#include "mymuduo/net/Callbacks.h"
#include "mymuduo/net/TimerId.h"

namespace mymuduo
{
    namespace net
    {

        class Channel;
        class Poller;
        class TimerQueue;

        class EventLoop : noncopyable
        {
        private:
            typedef std::function<void()> Functor;
            typedef std::vector<Channel *> ChannelList;

            bool looping_;
            bool quit_;
            bool callingPendingFunctors_;
            const pid_t threadId_; // EventLoop的构造函数会记住本对象所属的线程（threadId_）
            Timestamp pollReturnTime_;

            // 注意EventLoop通过scoped_ptr来间接持有 Poller
            boost::scoped_ptr<Poller> poller_;
            ChannelList activeChannels_;
            TimerQueue *timerQueue_;

            int wakeupFd_;
            // wakeupChannel_用于处理wakeupFd_上的readable事件，将事件分发至handleRead()函数
            boost::scoped_ptr<Channel> wakeupChannel_;
            // 只有pendingFunctors_暴露给了其他线程，因 此用mutex保护
            MutexLock mutex_;
            std::vector<Functor> pendingFunctors_; // pending - 悬而未决的，待定的

            void abortNotInLoopThread();

            void handleRead(); // Weaked up
            void doPendingFunctors();

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
             * EventLoop对象的生命期通常和其所属的线程一样长，它不必是heap对象
             **/
            void loop();
            void wakeup();
            void quit();
            void updateChannel(Channel *channel);
            void removeChannel(Channel *channel);
            bool hasChannel(Channel *channel);

            pid_t threadId() { return threadId_; }

            void runInLoop(Functor cb);
            void queueInLoop(Functor cb);
            void cancel(TimerId timerId);
            /**  Timer Event
             * 允许跨线程使用，比方说我想在某个IO线程中执行超时回调。
             * 这就带来 线程安全性方面的问题，muduo的解决办法不是加锁，
             * 而是把对 TimerQueue的操作转移到IO线程来进行 -> runInLoop
             */
            TimerId runAt(const Timestamp &time, const TimerCallback &cb);
            TimerId runAfter(double delay, const TimerCallback &cb);
            TimerId runEvery(double interval, const TimerCallback &cb);

            /*
             * muduo的接口设计会明确哪些成员函数是线程安全的，可以跨线程调用；
             * 哪些成员函数只能在某个特定线程调用（主要是IO线程）。
             * 为了能在运行时检查这些pre-condition，EventLoop提供了
             * isInLoopThread()和 assertInLoopThread()等函数
             * CurrentThread::tid() 中存储的是每个线程单独持有一份的 tid变量，而 threadId_存储的是 EventLoop从属的线程
             */
            bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

            /* 只能在所属线程执行，这里进行检查 */
            void assertInLoopThread()
            {
                if (!isInLoopThread())
                {
                    abortNotInLoopThread();
                }
            }

            /*
             * 返回该线程对应的唯一 EventLoop，作为静态成员函数，返回的指针也是每个线程共享的
             */
            static EventLoop *getEventLoopOfCurrentThread();
        };
    }
}

#endif // !MY_MUDUO_NET_EVENTLOOP_H