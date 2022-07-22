#ifndef MY_TCPCONNECTION_H
#define MY_TCPCONNECTION_H

#include "mymuduo/net/Buffer.h"
#include "mymuduo/net/Callbacks.h"
#include "mymuduo/net/InetAddress.h"
#include "mymuduo/base/noncopyable.h"
#include "mymuduo/base/StringPiece.h"
#include "mymuduo/base/Types.h"

#include <boost/scoped_ptr.hpp>

namespace mymuduo
{
    namespace net
    {
        class Socket;
        class Channel;
        class EventLoop;

        /**
         * TcpConnection是muduo里唯一默认使用shared_ptr来管理的class，
         * 也是唯一继承enable_shared_from_this的class，这源于其模糊的生命期
         * TcpConnection表示的是“一次TCP连接”，它是不可再生的，一旦连接断开，这个TcpConnection对象就没用了
         * 另外TcpConnection 没有发起连接的功能，其构造函数的参数是已经建立好连接的socket fd
         * （无论是TcpServer被动接受还是TcpClient主动发起），因此其初始状态是kConnecting
         */
        class TcpConnection : noncopyable,
                              public std::enable_shared_from_this<TcpConnection>
        {
        public:
            TcpConnection(EventLoop *loop, std::string &name, int sockfd, InetAddress localAddr, InetAddress peerAddr);
            ~TcpConnection();

            void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
            void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
            /**
             * TcpConnection class也新增了CloseCallback事件回调，但是这个回调是给TcpServer和TcpClient用的
             * 用于通知它们移除所持有的 TcpConnectionPtr，这不是给普通用户用的，
             * 普通用户继续使用 ConnectionCallback
             */
            void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }
            void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
            {
                highWaterMarkCallback_ = cb;
                highWaterMark_ = highWaterMark;
            }
            void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

            void connectEstablished();
            void connectDestroyed();
            void send(const std::string &message);
            void shutdown();
            void setTcpNoDelay(bool on);
            void forceClose();
            void forceCloseInLoop();

            bool connected() { return state_ == kConnected; }
            EventLoop *getLoop() { return loop_; }
            std::string name() { return name_; }
            InetAddress localAddr() { return localAddr_; }
            InetAddress peerAddr() { return peerAddr_; }

        private:
            /**
             *    ---(established)---<  Connecting
             *    |
             * Connected  ------------- shutdown() ------------>  Disconnecting
             *    |                                                     |
             *    ---(handleClose)--->  Disconnected <---(handleClose)---
             *
             */
            enum StateE
            {
                kConnecting,
                kConnected,
                kDisconnecting,
                kDisconnected
            };

            const char *stateToString() const;
            void setState(StateE s) { state_ = s; }
            void handleRead(Timestamp receiveTime);
            void handleWrite();
            void handleClose();
            void handleError();

            void sendInLoop(const std::string &message);
            void sendInLoop(const char *data, const size_t len);
            void shutdownInLoop();

            EventLoop *loop_;
            std::string name_;
            StateE state_;

            Buffer inputBuffer_;
            Buffer outputBuffer_;

            // TcpConnection拥有TCP socket，它 的析构函数会close(fd)（在Socket的析构函数中发生）
            boost::scoped_ptr<Socket> socket_;
            // TcpConnection使用Channel 来获得socket上的IO事件，它会自己处理writable事件，
            // 而把readable 事件通过MessageCallback传达给客户
            boost::scoped_ptr<Channel> channel_;

            InetAddress localAddr_;
            InetAddress peerAddr_;
            size_t highWaterMark_;

            CloseCallback closeCallback_;
            MessageCallback messageCallback_;
            // connectionCallback_ 会在连接成功建立和关闭的时候调用，两处触发
            ConnectionCallback connectionCallback_;
            // 如果发送缓冲区被清空，就调用它
            WriteCompleteCallback writeCompleteCallback_;
            /**
             * 如果用非阻塞的方式写一个proxy，proxy有C和S两个连接。
             * 只考虑server发给client的数据流（反过来也是一样），为了
             * 防止server发过来的数据撑爆C的输出缓冲区，一种做法是在C的
             * HighWaterMarkCallback中停止读取S的数据，而在C的
             * WriteCompleteCallback中恢复读取S的数据。
             * 这就跟用粗水管往水桶里灌水，用细水管从水桶中取水一个道理，
             * 上下两个水龙头要轮流开合，类似PWM
             */
            HighWaterMarkCallback highWaterMarkCallback_;
        };
    }
}

#endif //! MY_TCPCONNECTION_H