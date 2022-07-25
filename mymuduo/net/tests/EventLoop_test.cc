/****************************** 测试 EventLoop ********************************/
#include "mymuduo/net/EventLoop.h"
#include "mymuduo/base/CurrentThread.h"
#include "mymuduo/base/Thread.h"
#include <unistd.h>

using namespace mymuduo;
using namespace mymuduo::net;

/****************************** test 1 正常退出 ********************************/
// void threadFunc()
// {
//     printf("threadFunc(): pid = %d, tid = %d\n", getpid(), muduo::CurrentThread::tid());
//     EventLoop loop;
//     loop.loop();
// }

// int main(int argc, char const *argv[])
// {
//     printf("main(): pid = %d, tid = %d\n", getpid(), CurrentThread::tid());
//     EventLoop loop;
//     Thread thread(threadFunc);
//     thread.start();
//     loop.loop();
//     pthread_exit(NULL);
// }

/****************************** test 2 多个线程,断言退出 ********************************/
// EventLoop *g_loop;

// void threadFunc()
// {
//     g_loop->loop();
// }

// int main(int argc, char const *argv[])
// {
//     EventLoop loop;
//     g_loop = &loop;
//     Thread t(threadFunc);
//     t.start();
//     t.join();
//     return 0;
// }

/****************************** test 3 多个loop,构造函数输出信息 ********************************/
int main(int argc, char const *argv[])
{
    EventLoop loop1, loop2;
    return 0;
}