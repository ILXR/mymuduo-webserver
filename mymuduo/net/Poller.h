#ifndef MY_MUDUO_POLLER_H
#define MY_MUDUO_POLLER_H

#include <map>
#include <vector>

#include "mymuduo/net/EventLoop.h"
#include "mymuduo/base/Timestamp.h"

namespace mymuduo
{
    namespace net
    {

        class Channel;
        class EventLoop;

        // Poller class是IO multiplexing的封装
        // Poller是EventLoop的间接成员，只供其owner EventLoop在IO线程调用，因此无须加锁。其生命期与EventLoop相等
        class Poller
        {
        public:
            typedef std::vector<Channel *> ChannelList;
            Poller(EventLoop *loop);
            virtual ~Poller();

            // 必须由派生类来实现
            // poll 被 EventLoop 调用，用来获取触发事件的 Channel 列表，会放到调用方传入的 activeChannels中
            // 注意，在poll中不调用回调函数，因为后者可能会添加或删除 Channel，从而造成遍历期间改变了数组大小（对于pollpoller）
            virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
            // Channel 在析构时会从 Poller中删除自己
            virtual void removeChannel(Channel *channel) = 0;
            // 改变感兴趣的 IO 事件，必须在 loop thread 中调用
            virtual void updateChannel(Channel *channel) = 0;

            bool hasChannel(Channel *channel);
            void assertInLoopThread() const { ownerLoop_->assertInLoopThread(); }
            static Poller *newDefaultPoller(EventLoop *loop);

        protected:
            typedef std::map<int, Channel *> ChannelMap;
            // ChannelMap是从fd到Channel* 的映射
            // Poller并不拥有Channel，Channel在析构之前必须自己 unregister（EventLoop::removeChannel()），避免空悬指针
            ChannelMap channels_;

        private:
            EventLoop *ownerLoop_;
        };
    }
}

#endif // !MY_MUDUO_POLLER_H