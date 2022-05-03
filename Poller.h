#ifndef MY_POLLER_H
#define MY_POLLER_H

#include <vector>
#include <map>
#include <time.h>
#include <muduo/base/Timestamp.h>
#include "EventLoop.h"

struct pollfd;

namespace mymuduo
{
    using Timestamp = muduo::Timestamp;
    class Channel;

    /**
     * Poller class是IO multiplexing的封装
     * Poller是EventLoop的间接成员，只供其owner EventLoop在IO线程调用，
     * 因此无须加锁。其生命期与EventLoop相等。
     * Poller并不拥有Channel，Channel在析构之前必须自己 unregister
     * （EventLoop::removeChannel()），避免空悬指针。
     */
    class Poller
    {

    public:
        typedef std::vector<Channel *> ChannelList;

        Poller(EventLoop *loop);
        ~Poller();

        /**
         * Poller供EventLoop调用的函数目前有两个，poll()和 updateChannel()
         */
        muduo::Timestamp poll(int timeoutMs, ChannelList *activeChannels);
        void updateChannel(Channel *channel);

        void assertInLoopThread() { ownerLoop_->assertInLoopThread(); }

    private:
        void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
        // Poller::poll()不会在每次调用poll(2)之前临时构造pollfd数组， 而是把它缓存起来
        typedef std::vector<struct pollfd> PollFdList;
        // ChannelMap是从fd到Channel* 的映射
        typedef std::map<int, Channel *> ChannelMap;

        EventLoop *ownerLoop_;
        PollFdList pollfds_;
        ChannelMap channels_;
    };
}

#endif