#ifndef MY_CHANNEM_H
#define MY_CHANNEM_H

/*
 * hpp（Header Plus Plus）头文件，顾名思义就是 .h 文件加上 .cpp 文件，
 * 在 boost 开源库中频繁出现，其实就是 .cpp 实现代码混入 .h 文件当中，
 * 定义和实现都包含在同一个文件里。
 */
#include <boost/function.hpp>

namespace mymuduo
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

        Channel(EventLoop *loop, int fd);

        /*
         * Channel::handleEvent()是Channel的核心，它由EventLoop::loop()调用
         * 它的功能是根据revents_的值分别调用不同的用户回调
         */
        void handleEvent();
        /* Channel的成员函数都只能在IO线程调用，因此更新数据成员 都不必加锁 */
        void setReadCallback(const EventCallback &cb) { readCallback_ = cb; }
        void setWriteCallback(const EventCallback &cb) { writeCallback_ = cb; }
        void setErrorCallback(const EventCallback &cb) { errorCallback_ = cb; }

        int fd() { return fd_; }
        int events() { return events_; }
        void set_revents(int revt) { revents_ = revt; }
        bool isNoneEvents() { return events_ == kNoneEvent; }

        void enableReading()
        {
            events_ |= kReadEvent;
            update();
        }
        int index() { return index_; }
        void set_index(int idx) { index_ = idx; }
        EventLoop *ownerLoop() { return loop_; }
        ~Channel();

    private:
        /*
         * Channel::update()会调用EventLoop::updateChannel()，后者会转而调 用Poller::updateChannel()
         */
        void update();

        static const int kNoneEvent;
        static const int kReadEvent;
        static const int kWriteEvent;

        EventLoop *loop_;
        const int fd_;
        /* events_是它关心的IO事件， 由用户设置；
         * revents_是目前活动的事件，由EventLoop/Poller设置；
         * 这两个字段都是bit pattern，它们的名字来自poll(2)的struct pollfd
         */
        int events_, revents_;
        int index_; // used by Poller, 在fd数组中的下标
        EventCallback readCallback_, writeCallback_, errorCallback_;
    };
}

#endif MY_CHANNEM_H // !MY_CHANNEM_H