#ifndef MY_TCPCONNECTION_H
#define MY_TCPCONNECTION_H

#include "util.h"
#include "Callback.h"
#include "noncopyable.h"
#include <boost/scoped_ptr.hpp>

namespace mymuduo
{
    class Socket;
    class Channel;
    class EventLoop;
    class TcpConnection;

    /**
     * TcpConnection是muduo里唯一默认使用shared_ptr来管理的class，
     * 也是唯一继承enable_shared_from_this的class，这源于其模糊的生命期
     * TcpConnection表示的是“一次TCP连接”，它是不可再生的，一 旦连接断开，这个TcpConnection对象就没用了
     * 另外TcpConnection 没有发起连接的功能，其构造函数的参数是已经建立好连接的socket fd
     * （无论是TcpServer被动接受还是TcpClient主动发起），因此其初始状态是kConnecting
     */
    class TcpConnection : noncopyable,
                          public std::enable_shared_from_this<TcpConnection>
    {
    public:
        TcpConnection(EventLoop *loop, std::string &name, int sockfd, InetAddress localAddr, InetAddress peerAddr);
        ~TcpConnection();
        void setConnectionCallback(ConnectionCallback &cb)
        {
            connectionCallback_ = cb;
        }
        void setMessageCallback(MessageCallback &cb)
        {
            messageCallback_ = cb;
        }
        void connectEstablished(){}
        bool connected() { return state_ == kConnected; }
        std::string name() { return name_; }
        InetAddress localAddr() { return localAddr_; }
        InetAddress peerAddr() { return peerAddr_; }

    private:
        enum StateE
        {
            kConnecting,
            kConnected,
        };

        void setState(StateE s) { state_ = s; }
        void handleRead();

        EventLoop *loop_;
        std::string name_;
        StateE state_;

        // TcpConnection拥有TCP socket，它 的析构函数会close(fd)（在Socket的析构函数中发生）
        boost::scoped_ptr<Socket> socket_;
        // TcpConnection使用Channel 来获得socket上的IO事件，它会自己处理writable事件，
        // 而把readable事 件通过MessageCallback传达给客户
        boost::scoped_ptr<Channel> channel_;

        InetAddress localAddr_;
        InetAddress peerAddr_;
        ConnectionCallback connectionCallback_;
        MessageCallback messageCallback_;
    };
}

#endif //! MY_TCPCONNECTION_H