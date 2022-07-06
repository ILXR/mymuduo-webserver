#include <mymuduo/base/Thread.h>
#include <mymuduo/base/Logging.h>
#include <mymuduo/base/CountDownLatch.h>
#include <mymuduo/base/AsyncLogging.h>
#include <iostream>
#include <unistd.h>

using namespace mymuduo;

const int NUM = 10;
CountDownLatch countDown(NUM);
AsyncLogging LOGGING("logging_test", 1000, 1);
void out_to_file(const char *str, int len)
{
    LOGGING.append(str, len);
}

void func(int id)
{
    for (int i = 0; i < 3; i++)
    {
        sleep(1);
        LOG_INFO << "thread " << id << " - " << i;
    }
    countDown.countDown();
}

int main(int argc, char const *argv[])
{
    LOG_DEBUG << "test";
    Logger::setOutput(out_to_file);
    LOGGING.start();
    for (int i = 0; i < NUM; i++)
    {
        Thread tmp(std::function<void()>(std::bind(func, i)), "thread " + i);
        tmp.start();
    }
    countDown.wait();
    LOG_WARN << "Main Thread Exit";
    LOGGING.stop();
    return 0;
}