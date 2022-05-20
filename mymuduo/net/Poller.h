#ifndef MY_MUDUO_POLLER_H
#define MY_MUDUO_POLLER_H

#include <map>
#include <vector>

#include "mymuduo/base/Timestamp.h"

namespace mymuduo
{
    namespace net
    {

        class Channel;
        class EventLoop;
        class Poller
        {
        public:
            typedef std::vector<Channel *> ChannelList;
            Poller(EventLoop *loop);
            virtual ~Poller();

            // 必须由派生类来实现
            // poll 被 EventLoop 调用，用来获取触发事件的 Channel 列表
            virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
            virtual void removeChannel(Channel *channel) = 0;
            virtual void updateChannel(Channel *channel) = 0;
            
            bool hasChannel(Channel *channel);
            void assertInLoopThread() const;
            static Poller *newDefaultPoller(EventLoop *loop);

        protected:
            // ChannelMap是从fd到Channel* 的映射
            typedef std::map<int, Channel *> ChannelMap;
            ChannelMap channels_;

        private:
            EventLoop *ownerLoop_;
        };
    }
}

#endif // !MY_MUDUO_POLLER_H