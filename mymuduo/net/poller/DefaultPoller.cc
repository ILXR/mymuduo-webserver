#include "mymuduo/net/Poller.h"
#include "mymuduo/net/poller/EPollPoller.h"
#include "mymuduo/net/poller/PollPoller.h"

#include <stdlib.h>

using namespace mymuduo::net;

Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MYMUDUO_USE_POLL"))
    {
        return new PollPoller(loop);
    }
    else
    {
        return new EPollPoller(loop);
    }
}