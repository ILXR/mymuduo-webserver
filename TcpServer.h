#ifndef MY_TCP_SERVER_H
#define MY_TCP_SERVER_H

#include <map>
#include <boost/scoped_ptr.hpp>
#include <muduo/base/Atomic.h>

#include "Callback.h"
#include "Socket.h"

namespace mymuduo
{
    using muduo::AtomicInt32;
    class EventLoopThreadPool;
    class EventLoop;
    class Acceptor;
    /**
     * TcpServer class的功能是管理accept(2)获得的TcpConnection。
     * TcpServer是供用户直接使用的，生命期由用户控制
     */
    class TcpServer
    {
    private:
        typedef std::function<void(EventLoop *)> ThreadInitCallback;
        typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

        void newConnection(int sockfd, const InetAddress &peerAddr);

        EventLoop *loop_;
        /**
         * 每个TcpConnection对象有一个名字，这个名字是由其所属的
         * TcpServer在创建TcpConnection对象时生成，名字是ConnectionMap的 key
         */
        const std::string name_;

        /**
         * TcpServer内部使用Acceptor来获得新连接的fd。
         * 它保存用户提供的 ConnectionCallback和MessageCallback，
         * 在新建TcpConnection的时候会 原样传给后者
         */
        boost::scoped_ptr<Acceptor> acceptor_;
        /**
         * 目前的设计是每个 TcpServer有自己的EventLoopThreadPool，
         * 多个TcpServer之间不共享 EventLoopThreadPool
         */
        boost::scoped_ptr<EventLoopThreadPool> threadPool_;
        MessageCallback messageCallback_;
        ConnectionCallback connectionCallback_;
        WriteCompleteCallback writeCompleteCallback_;
        ThreadInitCallback threadInitCallback_;

        AtomicInt32 started_;
        int nextConnId_;
        /**
         * TcpServer持有目前存活的TcpConnection的 shared_ptr（定义为TcpConnectionPtr），
         * 因为TcpConnection对象的生命期是模糊的，用户也可以持有TcpConnectionPtr
         * 但是在内部实现中，只有这里是对 TcpConnection 进行了存储的，所以当erase之后计数就为1，随时会析构
         */
        ConnectionMap connections_;

        void removeConnection(const TcpConnectionPtr &conn);
        void removeConnectionInLoop(const TcpConnectionPtr &conn);

    public:
        TcpServer(EventLoop *loop,
                  const InetAddress &listenAddr,
                  const string &nameArg);
        ~TcpServer();

        void start();
        void setThreadNum(int numThreads);
        void setConnectionCallback(const ConnectionCallback &cb)
        {
            connectionCallback_ = cb;
        };
        void setMessageCallback(const MessageCallback &cb)
        {
            messageCallback_ = cb;
        };
        void setWriteCompleteCallback(const WriteCompleteCallback &cb)
        {
            writeCompleteCallback_ = cb;
        }
        void setThreadInitCallback(const ThreadInitCallback &cb)
        {
            threadInitCallback_ = cb;
        }
    };

}

#endif //! MY_TCP_SERVER_H