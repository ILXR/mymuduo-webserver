#include "EPoller.h"
#include "Channel.h"

#include <poll.h>
#include <boost/static_assert.hpp>

using namespace mymuduo;
using namespace mymuduo::net;

// 用来表示 channel->index() 的意义
namespace
{
    const int kNew = -1;
    const int kAdded = 1;
    const int kDeleted = 2;
}

BOOST_STATIC_ASSERT(EPOLLIN == POLLIN);
BOOST_STATIC_ASSERT(EPOLLPRI == POLLPRI);
BOOST_STATIC_ASSERT(EPOLLOUT == POLLOUT);
BOOST_STATIC_ASSERT(EPOLLRDHUP == POLLRDHUP);
BOOST_STATIC_ASSERT(EPOLLERR == POLLERR);
BOOST_STATIC_ASSERT(EPOLLHUP == POLLHUP);

EPoller::EPoller(EventLoop *loop)
    : ownerLoop_(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        perror("EPollPoller::EPollPoller\n");
        exit(1);
    }
}

EPoller::~EPoller()
{
    ::close(epollfd_);
}

Timestamp EPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                                 static_cast<int>(events_.size()), timeoutMs);
    Timestamp now(Timestamp::now());
    if (numEvents > 0)
    {
        printf("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (!implicit_cast<size_t>(numEvents) == events_.size())
        {
            // 如果当前活动fd的数目填满了events_，那么下次 就尝试接收更多的活动fd
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        // printf("nothing happened\n");
    }
    else
    {
        perror("EPoller::poll()\n");
    }
    return now;
}

// EPoller::fillActiveChannels()的功能是将events_中的活动fd填入 activeChannels
void EPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    assert(implicit_cast<size_t>(numEvents) <= events_.size());
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
#ifdef NDEBUG
        int fd = channel->fd();
        ChannelMap::const_iterator it = channels_.find(fd);
        assert(it != channels_.end());
        assert(it->second == channel);
#endif
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

void EPoller::updateChannel(Channel *channel)
{
    EPoller::assertInLoopThread();
    /**
     * 因为epoll是有状态 的，因此这两个函数要时刻维护内核中的fd状态与应用程序的状态相符，
     * Channel::index()和Channel::set_index()被挪用为标记此Channel是否 位于epoll的关注列表之中
     *
     */
    const int index = channel->index();
    //   LOG_TRACE << "fd = " << channel->fd()
    // << " events = " << channel->events() << " index = " << index;
    if (index == kNew || index == kDeleted)
    {
        // a new one, add with EPOLL_CTL_ADD
        int fd = channel->fd();
        if (index == kNew)
        {
            assert(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;
        }
        else // index == kDeleted
        {
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else
    {
        // update existing one with EPOLL_CTL_MOD/DEL
        int fd = channel->fd();
        (void)fd;
        assert(channels_.find(fd) != channels_.end());
        assert(channels_[fd] == channel);
        assert(index == kAdded);
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPoller::removeChannel(Channel *channel)
{
    EPoller::assertInLoopThread();
    int fd = channel->fd();
    // LOG_TRACE << "fd = " << fd;
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->isNoneEvent());
    int index = channel->index();
    assert(index == kAdded || index == kDeleted);
    size_t n = channels_.erase(fd);
    (void)n;
    assert(n == 1);

    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

void EPoller::update(int operation, Channel *channel)
{
    struct epoll_event event;
    memZero(&event, sizeof event);
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();
    // LOG_TRACE << "epoll_ctl op = " << operationToString(operation)
    //           << " fd = " << fd << " event = { " << channel->eventsToString() << " }";
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            // LOG_SYSERR << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
        }
        else
        {
            // LOG_SYSFATAL << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
        }
    }
}

const char *EPoller::operationToString(int op)
{
    switch (op)
    {
    case EPOLL_CTL_ADD:
        return "ADD";
    case EPOLL_CTL_DEL:
        return "DEL";
    case EPOLL_CTL_MOD:
        return "MOD";
    default:
        assert(false && "ERROR op");
        return "Unknown Operation";
    }
}

bool EPoller::hasChannel(Channel *channel) const
{
    assertInLoopThread();
    ChannelMap::const_iterator it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}