#ifndef MY_MUDUO_NET_CHANNEL_H
#define MY_MUDUO_NET_CHANNEL_H

/*
 * hpp（Header Plus Plus）头文件，顾名思义就是 .h 文件加上 .cpp 文件，
 * 在 boost 开源库中频繁出现，其实就是 .cpp 实现代码混入 .h 文件当中，
 * 定义和实现都包含在同一个文件里。
 */
#include <boost/function.hpp>
#include "mymuduo/base/Timestamp.h"

namespace mymuduo
{
    namespace net
    {

        class EventLoop;

        /*
         * 每个Channel对象自始至终只属于一个EventLoop， 因此每个Channel对象都只属于某一个IO线程。
         * 每个Channel对象自始至终只负责一个文件描述符（fd）的IO事件分发，但它并不拥有这个fd，
         * 也不会在析构的时候关闭这个fd。Channel会把不同的IO事件分发为不同的回调，
         * 例如ReadCallback、WriteCallback等，而且“回调”用 boost::function表示，
         * 用户无须继承Channel，Channel不是基类。
         * fd 可以是 socket, eventfd, timerfd, signalfd
         */
        class Channel
        {
        public:
            typedef boost::function<void()> EventCallback;
            typedef std::function<void(Timestamp)> ReadEventCallback;

            Channel(EventLoop *loop, int fd);
            ~Channel();

            /*
             * Channel::handleEvent()是Channel的核心，它由EventLoop::loop()调用
             * 它的功能是根据revents_的值分别调用不同的用户回调
             */
            void handleEvent(Timestamp receiveTime);
            /* Channel的成员函数都只能在IO线程调用，因此更新数据成员 都不必加锁 */
            void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
            void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
            void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
            void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

            int fd() { return fd_; }
            int events() { return events_; }
            int index() { return index_; }
            /**
             * 将channel与对象(owner object)绑定
             * 使得 owner object 在handleEvent中不被析构
             */
            void tie(const std::shared_ptr<void> &obj);
            void set_revents(int revt) { revents_ = revt; }
            void set_index(int idx) { index_ = idx; }
            void remove();

            void enableReading()
            {
                events_ |= kReadEvent;
                update();
            }
            void disableReading()
            {
                events_ &= ~kReadEvent;
                update();
            }
            void enableWriting()
            {
                events_ |= kWriteEvent;
                update();
            }
            void disableWriting()
            {
                events_ &= ~kWriteEvent;
                update();
            }
            void disableAll()
            {
                events_ = kNoneEvent;
                update();
            }
            bool isWriting() const { return events_ & kWriteEvent; }
            bool isReading() const { return events_ & kReadEvent; }
            bool isNoneEvent() { return events_ == kNoneEvent; }

            EventLoop *ownerLoop() { return loop_; }

        private:
            static string eventsToString(int fd, int ev);
            /*
             * Channel::update()会调用EventLoop::updateChannel()，后者会转而调 用Poller::updateChannel()
             */
            void update();

            static const int kNoneEvent;
            static const int kReadEvent;
            static const int kWriteEvent;

            // TcpConnection 弱引用
            std::weak_ptr<void> tie_;
            EventLoop *loop_;
            const int fd_;
            /* events_是它关心的IO事件， 由用户设置；
             * revents_是目前活动的事件，由EventLoop/Poller设置；
             * 这两个字段都是bit pattern，它们的名字来自poll(2)的struct pollfd
             */
            int events_, revents_;
            int index_; // used by Poller, 在fd数组中的下标
            bool tied_;
            bool addedToLoop_;
            bool eventHandling_;
            EventCallback writeCallback_, errorCallback_, closeCallback_;
            ReadEventCallback readCallback_;
        };
    } // net
} // mymuduo

#endif // !MY_MUDUO_NET_CHANNEL_H