#include "mymuduo/net/TcpServer.h"

#include "mymuduo/base/Logging.h"
#include "mymuduo/net/EventLoop.h"
#include "mymuduo/net/Acceptor.h"
#include "mymuduo/net/SocketsOps.h"
#include "mymuduo/net/InetAddress.h"
#include "mymuduo/net/TcpConnection.h"
#include "mymuduo/net/EventLoopThread.h"
#include "mymuduo/net/EventLoopThreadPool.h"

#include <iostream>
#include <functional>

using namespace mymuduo;
using namespace mymuduo::net;

TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const string &nameArg,
                     Option option)
    : loop_(loop),
      name_(nameArg),
      ipPort_(listenAddr.toIpPort()),
      acceptor_(new Acceptor(loop_, listenAddr)),
      threadPool_(new EventLoopThreadPool(loop, nameArg)),
      messageCallback_(defaultMessageCallback),
      connectionCallback_(defaultConnectionCallback),
      nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    if (started_.getAndSet(1) == 0)
    {
        threadPool_->start(threadInitCallback_);
        assert(!acceptor_->listening());
        loop_->runInLoop(
            std::bind(&Acceptor::listen, get_pointer(acceptor_)));
    }
}

/**
 * 在新连接到达时，Acceptor会回调newConnection()，后者会创建 TcpConnection对象conn，
 * 把它加入ConnectionMap，设置好callback，再调用conn->connectEstablished()，
 * 其中会回调用户提供的 ConnectionCallback
 */
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{

    loop_->assertInLoopThread();

    char buf[32];
    // C 库函数 int snprintf(char *str, size_t size, const char *format, ...)
    // 设将可变参数(...)按照 format 格式化成字符串，并将字符串复制到 str 中，
    // size 为要写入的字符的最大数目，超过 size 会被截断。
    // 会自动再末尾添加 '\0'
    snprintf(buf, sizeof buf, "#%d", nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO << "TcpServer::newConnection [" << name_
             << "] - new connection [" << connName
             << "] from " << peerAddr.toIpPort();
    InetAddress localAddr(sockets::getLocalAddr(sockfd));
    EventLoop *ioLoop = threadPool_->getNextLoop();

    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, _1));
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // ioLoop和loop_间的线程切换都发生在连接建立和断开的时刻，不影响正常业务的性能
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

/**
 * 把conn从ConnectionMap中移除。这时TcpConnection已经是命悬一线：
 *  如果用户不持有TcpConnectionPtr的话，conn的引用计数已降到1。
 *  因此使用 bind 将 conn 绑定到connectDestroyed，这样就可以保证 conn能够正常释放资源。
 * 注意这里一定要用EventLoop::queueInLoop()，否则有可能出现抢占调度，使得
 *  Channel::handleEvent()执行到一半的时候，其所属的Channel对象本身被销毁了。
 */
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_ << "] - connection " << conn->name();
    size_t n = connections_.erase(conn->name());
    assert(n == 1);
    (void)n;
    EventLoop *ioLoop = conn->getLoop();
    // 用boost::bind让TcpConnection的生命期长到调用 connectDestroyed()的时刻
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
