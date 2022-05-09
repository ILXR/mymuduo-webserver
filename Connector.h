#ifndef MY_CONNECTOR_H
#define MY_CONNECTOR_H

#include "Types.h"
#include "TimerId.h"
#include "InetAddress.h"
#include "noncopyable.h"

#include <boost/function.hpp>

namespace mymuduo
{
    namespace net
    {

        class Channel;
        class EventLoop;

        /**
         * Connector只负责建立socket连接，不负责创建TcpConnection，
         * 它的 NewConnectionCallback回调的参数是socket文件描述符。
         */
        class Connector : noncopyable,
                          public std::enable_shared_from_this<Connector>
        {
        public:
            typedef boost::function<void(int sockfd)> NewConnectionCallback;
            Connector(EventLoop *loop, const InetAddress &serverAddr);
            ~Connector();

            void setNewConnectionCallback(const NewConnectionCallback &cb) { newConnectionCallback_ = cb; }
            void start();   // in any thread
            void restart(); // int loop thread
            void stop();    // in any thread

            const InetAddress &serverAddress() const { return serverAddr_; }

        private:
            enum States
            {
                kDisconnected,
                kConnecting,
                kConnected
            };
            static const int kMaxRetryDelayMs = 30 * 1000;
            static const int kInitRetryDelayMs = 500;

            void setState(States s) { state_ = s; }
            void startInLoop();
            void stopInLoop();
            void connect();
            void connecting(int sockfd);
            void handleWrite();
            void handleError();
            void retry(int sockfd);
            int removeAndResetChannel();
            void resetChannel();

            EventLoop *loop_;
            InetAddress serverAddr_;
            TimerId timerId_;
            bool connect_; // atomic
            States state_; // FIXME: use atomic variable
            std::unique_ptr<Channel> channel_;
            NewConnectionCallback newConnectionCallback_;
            int retryDelayMs_;
        };
    }
}

#endif