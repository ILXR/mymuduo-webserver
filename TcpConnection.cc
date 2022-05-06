#include "TcpConnection.h"
#include "EventLoop.h"
#include "Channel.h"
#include "Socket.h"
#include "SocketsOps.h"

using namespace mymuduo;

TcpConnection::TcpConnection(EventLoop *loop, std::string &name,
                             int sockfd, InetAddress localAddr, InetAddress peerAddr)
    : loop_(loop),
      name_(name),
      state_(kConnected),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop_, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr)
{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this));
}

TcpConnection::~TcpConnection()
{
}

void TcpConnection::handleRead()
{
    char buf[65536];
    ssize_t n = ::read(channel_->fd(), buf, sizeof buf);
    if (n > 0)
    {
        messageCallback_(shared_from_this(), buf, n);
    }
    else if (n == 0)
    {
        // 读出长度为0 说明连接被断开了
        handleClose();
    }
    else
    {
        handleError();
    }
}

void TcpConnection::handleWrite()
{
}

/**
 * TcpConnection::handleClose()的主要功能是调用closeCallback_，
 * 这个回调绑定到TcpServer::removeConnection()
 */
void TcpConnection::handleClose()
{
    loop_->assertInLoopThread();
    printf("TcpConnection::handleClose state = %d", state_);
    assert(state_ == kConnected);
    channel_->disableAll();
    closeCallback_(shared_from_this());
}

void TcpConnection::handleError()
{
    int err = sockets::getSocketError(channel_->fd());
    printf("TcpConnection::handleError [%s] - SO_ERROR = %d , %s", name_.c_str(), err, strerror(err));
}

void TcpConnection::connectEstablished()
{
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    connectionCallback_(shared_from_this());
}

/**
 * TcpConnection::connectDestroyed()是TcpConnection析构前最后调用的一个成员函数，它通知用户连接已断开；
 * 部分与 handleClose 重复，这是因为在某些情况下可以不经由handleClose()而直接调用 connectDestroyed()
 * （比如程序突然退出时）
 */
void TcpConnection::connectDestroyed()
{
    loop_->assertInLoopThread();
    assert(state_ == kConnected);
    setState(kDisconnected);
    channel_->disableAll();
    connectionCallback_(shared_from_this());
    loop_->removeChannel(get_pointer(channel_));
}