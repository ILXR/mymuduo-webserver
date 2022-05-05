#include "TcpConnection.h"
#include "Channel.h"
#include "Socket.h"

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
    messageCallback_(shared_from_this(), buf, n);
}