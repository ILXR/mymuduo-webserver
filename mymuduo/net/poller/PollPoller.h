#ifndef MY_MUDUO_NET_POLLPOLLER_H
#define MY_MUDUO_NET_POLLPOLLER_H

#include "mymuduo/net/Poller.h"

struct pollfd;

namespace mymuduo
{
    namespace net
    {

        /**
         * PollPoller class是IO multiplexing的封装
         * Poller是EventLoop的间接成员，只供其owner EventLoop在IO线程调用，
         * 因此无须加锁。其生命期与EventLoop相等。
         * PollPoller并不拥有Channel，Channel在析构之前必须自己 unregister
         * （EventLoop::removeChannel()），避免空悬指针。
         */
        class PollPoller : public Poller
        {

        public:
            PollPoller(EventLoop *loop);
            ~PollPoller();

            Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
            void updateChannel(Channel *channel) override;
            void removeChannel(Channel *channel) override;

        private:
            void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
            // PollPoller::poll()不会在每次调用poll(2)之前临时构造pollfd数组， 而是把它缓存起来
            typedef std::vector<struct pollfd> PollFdList;

            PollFdList pollfds_;
        };
    }
}

#endif // !MY_MUDUO_NET_POLLPOLLER_H