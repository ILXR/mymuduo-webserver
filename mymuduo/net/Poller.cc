#include "mymuduo/net/Poller.h"

#include "mymuduo/net/Channel.h"
#include "mymuduo/net/EventLoop.h"

using namespace mymuduo;
using namespace mymuduo::net;

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}
Poller::~Poller() = default;
bool Poller::hasChannel(Channel *channel)
{
    assertInLoopThread();
    ChannelMap::const_iterator it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}