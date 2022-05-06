#include "TcpConnection.h"
#include "EventLoop.h"
#include "Channel.h"
#include "Socket.h"
#include "SocketsOps.h"

using namespace mymuduo;

void mymuduo::defaultConnectionCallback(const TcpConnectionPtr &conn)
{
    printf("%s is %s\n", conn->localAddr().toIpPort().c_str(), conn->connected() ? "UP" : "DOWN");
}
void mymuduo::defaultMessageCallback(const TcpConnectionPtr &conn,
                                     Buffer *buffer,
                                     Timestamp receiveTime)
{
    buffer->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop *loop, std::string &name,
                             int sockfd, InetAddress localAddr, InetAddress peerAddr)
    : loop_(loop),
      name_(name),
      state_(kConnecting),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop_, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      inputBuffer_()
{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, _1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
}

TcpConnection::~TcpConnection()
{
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    loop_->assertInLoopThread();
    int savedErrno = 0;
    /*
    一是使用了scatter/gather IO（DMA 链表式传输），并且一部分缓冲区取自stack，这样输入缓冲区足够大，
    通常一次readv(2)调用就能取完全部数据。
    由于输入缓冲区足够大，也节省了一次 ioctl(socketFd, FIONREAD, &length)系统调用，
    不必事先知道有多少数据可读而提前预留（reserve()）Buffer的capacity()，
    可以在一次读取之后 将extrabuf中的数据append()给Buffer。

    二是Buffer::readFd()只调用一次read(2)，而没有反复调用read(2)直到其返回EAGAIN。
    首先，这么做是正确的，因为muduo采用level trigger，这么做不会丢失数据或消息。
    其次，对追求低延迟的程序来说，这么做是高效的，因为每次读数据只需要一次系统调用。
    再次，这样做照顾了多个连接的公平性，不会因为某个连接上数据量过大而影响 其他连接处理消息。
    假如muduo采用edge trigger，那么每次handleRead()至少调用两次 read(2)，
    平均起来比level trigger多一次系统调用，edge trigger不见得更高效。
    将来的一个改进措施是：
        如果n == writable＋sizeof extrabuf，就再读一次
    */
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        // 读出长度为0 说明连接被断开了
        handleClose();
    }
    else
    {
        errno = savedErrno;
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    if (channel_->isWriting())
    {
        // 如果第一次write(2)没有能够发送完全部数据的话，第二次调用write(2)几乎肯定会返回EAGAIN。
        // 在非阻塞模式下调用了阻塞操作，在该操作没有完成就返回这个错误，
        // 这个错误不会破坏socket的同步，不用管它，下次循环接着recv就可以。对非阻塞socket而言，EAGAIN不是一种错误
        // 因此muduo决定节省一次系统调用，这么做不影响程序的正确性，却能降低延迟
        ssize_t n = ::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                // 一旦发送完毕，立刻停止观察writable事件，避免busy loop
                channel_->disableWriting();
                // 该状态表示连接需要关闭，但是还未写完数据，因此写端在此关闭
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
            else
            {
                printf("I am going to write more data\n");
            }
        }
        else
        {
            // 一旦发生错误，handleRead()会读到0字节，继而关闭连接
            perror("TcpConnection::handleWrite\n");
        }
    }
    else
    {
        printf("Connection is down, no more writing\n");
    }
}

/**
 * TcpConnection::handleClose()的主要功能是调用closeCallback_，
 * 这个回调绑定到TcpServer::removeConnection()
 */
void TcpConnection::handleClose()
{
    loop_->assertInLoopThread();
    printf("TcpConnection::handleClose state = %d\n", state_);
    assert(state_ == kConnected || state_ == kDisconnecting);
    channel_->disableAll();
    closeCallback_(shared_from_this());
}

void TcpConnection::handleError()
{
    int err = sockets::getSocketError(channel_->fd());
    printf("TcpConnection::handleError [%s] - SO_ERROR = %d , %s\n", name_.c_str(), err, strerror(err));
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
    assert(state_ == kConnected || state_ == kDisconnecting);
    setState(kDisconnected);
    channel_->disableAll();
    connectionCallback_(shared_from_this());
    loop_->removeChannel(get_pointer(channel_));
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}
void TcpConnection::shutdownInLoop()
{
    loop_->assertInLoopThread();
    if (!channel_->isWriting())
    {
        socket_->shutdownWrite();
    }
}

void TcpConnection::send(const std::string &message)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(message);
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message));
        }
    }
}

/**
 * sendInLoop()会先尝试直接发送数据，如果一次发送完毕就不会启 用WriteCallback；
 * 如果只发送了部分数据，则把剩余的数据放入 outputBuffer_，并开始关注writable事件，
 * 以后在handlerWrite()中发送剩 余的数据。
 * 如果当前outputBuffer_已经有待发送的数据，那么就不能先尝试发送了，因为这会造成数据乱序
 */
void TcpConnection::sendInLoop(const std::string &message)
{
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        // 没有在发数据
        nwrote = ::write(channel_->fd(), message.data(), message.size());
        if (nwrote >= 0)
        {
            if (implicit_cast<size_t>(nwrote) < message.size())
            {
                printf("I am going to write more data\n");
            }
        }
        else
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                perror("TcpConnection::sendInLoop\n");
            }
        }
    }
    assert(nwrote >= 0);
    if (implicit_cast<size_t>(nwrote) < message.size())
    {
        outputBuffer_.append(message.data() + nwrote, message.size() - nwrote);
        if (!channel_->isWriting())
        {
            // 监听可写事件
            channel_->enableWriting();
        }
    }
}