#include "TcpServer.h"
#include "EventLoop.h"
#include "EventLoopThread.h"
#include "SocketsOps.h"
#include "Acceptor.h"
#include "TcpConnection.h"
#include <iostream>

using namespace mymuduo;

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr)
    : loop_(loop)
{
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
              << "] from " << peerAddr.toIpPort();
    InetAddress localAddr(mymuduo::sockets::getLocalAddr(sockfd));
    TcpConnectionPtr conn(new TcpConnection(loop_,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->connectEstablished();
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        // conn->getLoop()->runInLoop(
        //   std::bind(&TcpConnection::connectDestroyed, conn));
    }
}