#include "TcpServer.h"
#include "EventLoop.h"
#include "EventLoopThread.h"
#include "SocketsOps.h"
#include "Acceptor.h"
#include "TcpConnection.h"
#include <iostream>
#include <functional>

using namespace mymuduo;

TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const string &nameArg)
    : loop_(loop),
      name_(nameArg),
      acceptor_(new Acceptor(loop_, listenAddr)),
      connectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback),
      nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, _1, _2));
}

void TcpServer::start()
{
    assert(!acceptor_->listenning());
    loop_->runInLoop(std::bind(&Acceptor::listen, get_pointer(acceptor_)));
    // if (started_.getAndSet(1) == 0)
    // {
    //     threadPool_->start(threadInitCallback_);

    //     assert(!acceptor_->listening());
    //     loop_->runInLoop(
    //         std::bind(&Acceptor::listen, get_pointer(acceptor_)));
    // }
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
    snprintf(buf, sizeof buf, "#%d", nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    std::cout << "TcpServer::newConnection [" << name_
              << "] - new connection [" << connName
              << "] from " << peerAddr.toIpPort() << endl;
    InetAddress localAddr(mymuduo::sockets::getLocalAddr(sockfd));
    TcpConnectionPtr conn(new TcpConnection(loop_,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, _1));
    conn->connectEstablished();
}

/**
 * 把conn从ConnectionMap中移除。这 时TcpConnection已经是命悬一线：
 *  如果用户不持有TcpConnectionPtr的话，conn的引用计数已降到1。
 * 注意这里一定要用 EventLoop::queueInLoop()，否则有可能出现：
 *  Channel::handleEvent()执行到一半的时候，其所属的Channel对象本身被销毁了
 */
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();
    printf("TcpServer::removeConnection [%s] - connection %s\n", name_.c_str(), conn->name().c_str());
    size_t n = connections_.erase(conn->name());
    assert(n == 1);
    (void)n;
    // 用boost::bind让TcpConnection的生命期长到调用 connectDestroyed()的时刻
    loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}