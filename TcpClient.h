#ifndef MY_TCP_CLIENT_H
#define MY_TCP_CLIENT_H

#include "Mutex.h"
#include "noncopyable.h"

namespace mymuduo
{
    namespace net
    {
        class EventLoop;
        class Connector;
        class InetAddress;
        typedef std::shared_ptr<Connector> ConnectorPtr;

        class TcpClient : noncopyable
        {
        private:
            /// Not thread safe, but in loop
            void newConnection(int sockfd);
            /// Not thread safe, but in loop
            void removeConnection(const TcpConnectionPtr &conn);

            EventLoop *loop_;
            ConnectorPtr connector_; // avoid revealing Connector
            const string name_;
            ConnectionCallback connectionCallback_;
            MessageCallback messageCallback_;
            WriteCompleteCallback writeCompleteCallback_;

            bool retry_;   // atomic
            bool connect_; // atomic
            // always in loop thread
            int nextConnId_;
            mutable MutexLock mutex_;
            // 每个TcpClient只管理一个TcpConnection
            TcpConnectionPtr connection_ GUARDED_BY(mutex_);

        public:
            TcpClient(EventLoop *loop,
                      const InetAddress &serverAddr,
                      const string &nameArg);
            ~TcpClient();

            void connect();
            void disconnect();
            void stop();

            TcpConnectionPtr connection() const
            {
                MutexLockGuard lock(mutex_);
                return connection_;
            }

            EventLoop *getLoop() const { return loop_; }
            bool retry() const { return retry_; }
            void enableRetry() { retry_ = true; }

            const string &name() const { return name_; }

            void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
            void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
            void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }
        };
    }
}

#endif // !MY_TCP_CLIENT_H