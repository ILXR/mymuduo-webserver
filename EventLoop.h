#ifndef MY_EVENT_LOOP_H
#define MY_EVENT_LOOP_H

#include "noncopyable.h"

namespace mymuduo
{

    class EventLoop : noncopyable
    {
    private:
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
    };

    EventLoop::EventLoop()
    {
    }

    EventLoop::~EventLoop()
    {
    }
};

#endif // !MY_EVENT_LOOP_H