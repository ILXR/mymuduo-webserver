#include "Channel.h"
#include "EventLoop.h"
#include "util.h"
#include <poll.h>
#include <stdio.h>

using namespace mymuduo;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fdArg) : loop_(loop),
                                               fd_(fdArg),
                                               events_(0),
                                               revents_(0),
                                               index_(-1)
{
}

Channel::~Channel() {}

void Channel::update()
{
    loop_->updateChannel(this);
}

void Channel::handleEvent()
{
    if (revents_ & POLLNVAL)
    {
        printf("Channel::handle_event() POLLNVAL\n");
    }
    if (revents_ & (POLLERR | POLLNVAL))
    {
        if (errorCallback_)
            errorCallback_();
    }
    if (revents_ & (POLLIN | POLLPRI | POLLHUP))
    {
        if (readCallback_)
            readCallback_();
    }
    if (revents_ & POLLOUT)
    {
        if (writeCallback_)
            writeCallback_();
    }
}