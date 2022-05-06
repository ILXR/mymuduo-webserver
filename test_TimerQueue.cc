#include "EventLoopThread.h"
#include "EventLoop.h"
#include "CurrentThread.h"
#include "TimerId.h"
#include "Callback.h"
#include "Timestamp.h"
#include <stdio.h>
#include <unistd.h>
#include <functional>

using namespace mymuduo;

int cnt = 0;
EventLoop *g_loop;

void printTid()
{
  printf("pid = %d, tid = %d\n", getpid(), CurrentThread::tid());
  printf("now %s\n", Timestamp::now().toString().c_str());
}

void print(const char *msg)
{
  printf("msg %s %s\n", Timestamp::now().toString().c_str(), msg);
  if (++cnt == 20)
  {
    g_loop->quit();
  }
}

void cancel(TimerId timer)
{
  g_loop->cancel(timer);
  printf("cancelled at %s\n", Timestamp::now().toString().c_str());
}

int main()
{
  printTid();
  sleep(1);
  {
    EventLoop loop;
    g_loop = &loop;

    print("main\n");
    loop.runAfter(1, std::bind(print, "once1\n"));
    loop.runAfter(1.5, std::bind(print, "once1.5\n"));
    loop.runAfter(2.5, std::bind(print, "once2.5\n"));
    loop.runAfter(3.5, std::bind(print, "once3.5\n"));
    TimerId t45 = loop.runAfter(4.5, std::bind(print, "once4.5\n"));
    loop.runAfter(4.2, std::bind(cancel, t45));
    loop.runAfter(4.8, std::bind(cancel, t45));
    loop.runEvery(2, std::bind(print, "every2\n"));
    TimerId t3 = loop.runEvery(3, std::bind(print, "every3\n"));
    loop.runAfter(9.001, std::bind(cancel, t3));

    loop.loop();
    print("main loop exits");
  }
  sleep(1);
  {
    mymuduo::EventLoopThread loopThread;
    EventLoop *loop = loopThread.startLoop();
    loop->runAfter(2, printTid);
    sleep(3);
    print("thread loop exits");
  }
}
