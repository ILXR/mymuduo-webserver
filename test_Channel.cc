/****************************** 单次触发的定时器 ********************************/
#include <sys/timerfd.h>
#include <functional>
#include "Channel.h"
#include "EventLoop.h"

mymuduo::EventLoop *g_loop;

void timeout(int timerfd)
{
    // 由于poll(2)是level trigger，在timeout()中应该read() timefd，否则下 次会立刻触发
    printf("Timeout!\n");
    static char *buffer = new char[1024];
    read(timerfd, (void *)buffer, 1024);
    // g_loop->quit();
}

int main(int argc, char const *argv[])
{
    mymuduo::EventLoop loop;
    g_loop = &loop;

    // 进程可以通过调用timer_create()创建特定的定时器，定时器是每个进程自己的，不是在fork时继承的，不会被传递给子进程
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    // 读timerfd的读取类型是uint64_t，可以直接使用，如果编译报错加头文件stdint.h。
    // timerfd读出来的值一般是1，表示超时次数
    mymuduo::Channel channel(&loop, timerfd);
    channel.setReadCallback(std::bind(timeout, timerfd));
    ;
    channel.enableReading();

    // struct timespec
    // {
    //     time_t tv_sec; /* Seconds */
    //     long tv_nsec;  /* Nanoseconds */
    // };
    // struct itimerspec
    // {
    //     struct timespec it_interval; /*定时间隔周期*/
    //     struct timespec it_value;    /* Initial expiration (第一次定时/到时/超时时间)*/
    // };
    // it_interval不为0则表示是周期性定时器，第一次之后每间隔该时间触发一次
    // it_value不为0，则表示延迟it_value后触发第一次
    // it_value和it_interval都为0表示停止定时器
    struct itimerspec howlong;
    bzero(&howlong, sizeof howlong);
    howlong.it_interval.tv_sec = 2;
    howlong.it_value.tv_sec = 1;
    ::timerfd_settime(timerfd, 0, &howlong, NULL);

    loop.loop();
    ::close(timerfd);
}