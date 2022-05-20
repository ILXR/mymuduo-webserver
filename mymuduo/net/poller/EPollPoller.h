#ifndef MY_MUDUO_NET_EPOLLPOLLER_H
#define MY_MUDUO_NET_EPOLLPOLLER_H

#include "mymuduo/net/Poller.h"

#include <sys/epoll.h>

namespace mymuduo
{
    namespace net
    {

        /**
         * epoll(4)是Linux独有的高效的IO multiplexing机制，它与poll(2)的不同之处
         * 主要在于poll(2)每次返回整个文件描述符数组，用户代码需要遍历数组以找到哪些
         * 文件描述符上有IO事件（Poller::fillActiveChannels()），
         * 而epoll_wait(2)返回的是活动fd的列表， 需要遍历的数组通常会小得多。
         * 在并发连接数较大而活动连接比例不高时，epoll(4)比poll(2)更高效
         */
        class EPollPoller : public Poller
        {
        private:
            // epoll_data是个union，muduo使用的是其ptr成员，用于存放Channel*，这样可以减少一步look up
            typedef std::vector<struct epoll_event> EventList;

            static const int kInitEventListSize = 16;
            static const char *operationToString(int op);

            void update(int operation, Channel *channel);
            void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

            int epollfd_;
            EventList events_;

        public:
            EPollPoller(EventLoop *loop);
            ~EPollPoller() override;

            Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
            void updateChannel(Channel *channel) override;
            void removeChannel(Channel *channel) override;
        };
    }
}

#endif //! MY_MUDUO_NET_EPOLLPOLLER_H