#ifndef MY_TCP_SERVER_H
#define MY_TCP_SERVER_H

#include <map>
#include <boost/scoped_ptr.hpp>

#include "mymuduo/base/Atomic.h"
/**
 * boost::scoped_ptr的实现和std::auto_ptr非常类似，都是利用了一个栈上的对象去管理一个堆上的对象，
 * 从而使得堆上的对象随着栈上的对象销毁时自动删除。
 * 不同的是，boost::scoped_ptr有着更严格的使用限制——不能拷贝。
 * 这就意味着：boost::scoped_ptr指针是不能转换其所有权的。
 * 1. 不能转换所有权
 *      boost::scoped_ptr所管理的对象生命周期仅仅局限于一个区间（该指针所在的"{}"之间），
 *      无法传到区间之外，这就意味着boost::scoped_ptr对象是不能作为函数的返回值的
 *      （std::auto_ptr可以）。
 * 2. 不能共享所有权
 *      这点和std::auto_ptr类似。这个特点一方面使得该指针简单易用。另一方面也造成了
 *      功能的薄弱——不能用于stl的容器中。
 * 3. 不能用于管理数组对象
 *      由于boost::scoped_ptr是通过delete来删除所管理对象的，而数组对象必须通过deletep[]来删除，
 *      因此boost::scoped_ptr是不能管理数组对象的，
 *      如果要管理数组对象需要使用boost::scoped_array类。
 */
#include "mymuduo/base/Types.h"
#include "mymuduo/net/TcpConnection.h"

namespace mymuduo
{
    namespace net
    {
        class EventLoopThreadPool;
        class EventLoop;
        class Acceptor;

        /**
         * TcpServer class的功能是管理accept(2)获得的TcpConnection。
         * TcpServer是供用户直接使用的，生命期由用户控制
         */
        class TcpServer
        {
        public:
            typedef std::function<void(EventLoop *)> ThreadInitCallback;
            enum Option
            {
                kNoReusePort,
                kReusePort,
            };

            TcpServer(EventLoop *loop,
                      const InetAddress &listenAddr,
                      const string &nameArg,
                      Option option = kNoReusePort);
            ~TcpServer();

            const string &ipPort() const { return ipPort_; }
            const string &name() const { return name_; }
            EventLoop *getLoop() const { return loop_; }

            void start();
            void setThreadNum(int numThreads);
            void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; };
            void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; };
            void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
            void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
            std::shared_ptr<EventLoopThreadPool> threadPool() { return threadPool_; }

        private:
            typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

            void newConnection(int sockfd, const InetAddress &peerAddr);
            void removeConnection(const TcpConnectionPtr &conn);
            void removeConnectionInLoop(const TcpConnectionPtr &conn);

            EventLoop *loop_;
            /**
             * 每个TcpConnection对象有一个名字，这个名字是由其所属的
             * TcpServer在创建TcpConnection对象时生成，名字是ConnectionMap的 key
             */
            const std::string name_;
            const string ipPort_;
            /**
             * TcpServer内部使用Acceptor来获得新连接的fd。
             * 它保存用户提供的 ConnectionCallback和MessageCallback，
             * 在新建TcpConnection的时候会原样传给后者
             */
            boost::scoped_ptr<Acceptor> acceptor_;
            /**
             * 目前的设计是每个 TcpServer有自己的EventLoopThreadPool，
             * 多个TcpServer之间不享 EventLoopThreadPool
             */
            // boost::scoped_ptr<EventLoopThreadPool> threadPool_;
            std::shared_ptr<EventLoopThreadPool> threadPool_;

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
             * 这时就需要使用 std::bind 来延长其生命周期，直到完成 onDestroyed
             */
            ConnectionMap connections_;
        };
    }
}

#endif //! MY_TCP_SERVER_H