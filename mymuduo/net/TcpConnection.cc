#include "mymuduo/net/TcpConnection.h"

#include "mymuduo/base/Logging.h"
#include "mymuduo/net/SocketsOps.h"
#include "mymuduo/net/EventLoop.h"
#include "mymuduo/net/Channel.h"
#include "mymuduo/net/Socket.h"

#include <sys/sendfile.h>

using namespace mymuduo;
using namespace mymuduo::net;

void mymuduo::net::defaultConnectionCallback(const TcpConnectionPtr &conn)
{
    LOG_TRACE << conn->localAddr().toIpPort() << " -> "
              << conn->peerAddr().toIpPort() << " is "
              << (conn->connected() ? "UP" : "DOWN");
}
void mymuduo::net::defaultMessageCallback(const TcpConnectionPtr &conn,
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
      inputBuffer_(),
      sendFd_(-1),
      sendLen_(0),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop_, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024)
{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, _1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
    LOG_DEBUG << "TcpConnection::ctor[" << name_ << "] at " << this
              << " fd=" << sockfd;
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_DEBUG << "TcpConnection::dtor[" << name_ << "] at " << this
              << " fd=" << channel_->fd()
              << " state=" << stateToString();
    assert(state_ == kDisconnected);
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
        LOG_SYSERR << "TcpConnection::handleRead";
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    // 需要同时处理 sendBuffer 和 sendFile
    // 由于在 http 连接流程中，sendFile 前可能有 buffer 数据还在 socket缓冲区
    // 而发送 buffer 数据时，file 数据一定已经被对方接收了
    // 所以优先处理 buffer 数据的发送
    if (channel_->isWriting())
    {
        if (outputBuffer_.readableBytes())
        {
            // 如果第一次write(2)没有能够发送完全部数据的话，第二次调用write(2)几乎肯定会返回EAGAIN。
            // 在非阻塞模式下调用了阻塞操作，在该操作没有完成就返回这个错误，
            // 这个错误不会破坏socket的同步，不用管它，下次循环接着recv就可以。对非阻塞socket而言，EAGAIN不是一种错误
            // 因此muduo决定节省一次系统调用，这么做不影响程序的正确性，却能降低延迟
            ssize_t n = ::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());
            if (n > 0)
            {
                outputBuffer_.retrieve(n);
                if (outputBuffer_.readableBytes() == 0 && sendLen_ == 0)
                {
                    // 一旦发送完毕，立刻停止观察writable事件，避免busy loop
                    channel_->disableWriting();
                    if (writeCompleteCallback_)
                    {
                        loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                    }
                    // 该状态表示连接需要关闭，但是还未写完数据，因此写端在此关闭
                    if (state_ == kDisconnecting)
                    {
                        shutdownInLoop();
                    }
                }
            }
            else
            {
                // 一旦发生错误，handleRead()会读到0字节，继而关闭连接
                LOG_SYSERR << "TcpConnection::handleWrite";
            }
        }
        else
        {
            ssize_t n = ::sendfile(socket_->fd(), sendFd_, NULL, sendLen_);
            if (n > 0)
            {
                sendLen_ -= n;
                if (sendLen_ == 0)
                {
                    channel_->disableWriting();
                    ::close(sendFd_);
                    sendFd_ = -1;
                    if (state_ == kDisconnecting)
                    {
                        shutdownInLoop();
                    }
                }
            }
            else
            {

                LOG_SYSERR << "TcpConnection::handleWrite send file len = "
                           << n << " remain len = " << sendLen_;
                ::close(sendFd_);
                sendFd_ = -1;
                sendLen_ = 0;
                channel_->disableWriting();
            }
        }
    }
    else
    {
        LOG_TRACE << "Connection fd = " << channel_->fd()
                  << " is down, no more writing";
    }
}

/**
 * TcpConnection::handleClose()的主要功能是调用closeCallback_，
 * 这个回调绑定到TcpServer::removeConnection()
 */
void TcpConnection::handleClose()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
    assert(state_ == kConnected || state_ == kDisconnecting);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());
    connectionCallback_(guardThis);
    // must be the last line
    closeCallback_(guardThis);
}

void TcpConnection::handleError()
{
    int err = sockets::getSocketError(channel_->fd());
    LOG_ERROR << "TcpConnection::handleError [" << name_
              << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

void TcpConnection::connectEstablished()
{
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);
    // 建立连接之后就和 Channel 绑定起来，这样 TcpConnection 的生命周期一定长于 Channel
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
    LOG_TRACE << stateToString();
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
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

void TcpConnection::forceClose()
{
    // FIXME: use compare and swap
    if (state_ == kConnected || state_ == kDisconnecting)
    {
        setState(kDisconnecting);
        loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

void TcpConnection::forceCloseInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kDisconnecting)
    {
        // as if we received 0 byte in handleRead();
        handleClose();
    }
}

void TcpConnection::sendFile(const int fd, const size_t count)
{
    if (state_ == kConnected)
    {
        this->sendFd_ = fd;
        this->sendLen_ = count;
        if (loop_->isInLoopThread())
            sendFileInLoop();
        else
            loop_->runInLoop(std::bind(&TcpConnection::sendFileInLoop, this));
    }
}

void TcpConnection::sendFileInLoop()
{
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    bool faultError = false;
    if (state_ == kDisconnected)
    {
        LOG_WARN << "disconnected, give up sending file";
        return;
    }
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        // 没有在发缓冲区数据/文件数据时才可以发送
        nwrote = ::sendfile(socket_->fd(), sendFd_, nullptr, sendLen_);
        if (nwrote >= 0)
        {
            sendLen_ -= nwrote;
            if (sendLen_ == 0)
                ::close(sendFd_);
        }
        else
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_SYSERR << "TcpConnection::sendFileInLoop";
                if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
                {
                    faultError = true;
                }
            }
        }
    }
    if (!faultError && sendLen_ > 0)
    {
        if (!channel_->isWriting())
        {
            // 监听可写事件
            channel_->enableWriting();
        }
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
            void (TcpConnection::*fp)(const std::string &message) = &TcpConnection::sendInLoop;
            loop_->runInLoop(std::bind(fp, this, message));
        }
    }
}

// FIXME efficiency!!!
void TcpConnection::send(Buffer *buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
        }
        else
        {
            void (TcpConnection::*fp)(const StringPiece &message) = &TcpConnection::sendInLoop;
            loop_->runInLoop(
                std::bind(fp,
                          this, // FIXME
                          buf->retrieveAllAsString()));
            // std::forward<string>(message)));
        }
    }
}

void TcpConnection::sendInLoop(const std::string &message)
{
    sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const StringPiece &message)
{
    sendInLoop(message.data(), message.size());
}

/**
 * sendInLoop()会先尝试直接发送数据，如果一次发送完毕就不会启用 WriteCallback；
 * 如果只发送了部分数据，则把剩余的数据放入 outputBuffer_，并开始关注writable事件，
 * 以后在handlerWrite()中发送剩余的数据。
 * 如果当前outputBuffer_已经有待发送的数据，那么就不能先尝试发送了，因为这会造成数据乱序
 */
void TcpConnection::sendInLoop(const char *data, const size_t len)
{
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;
    if (state_ == kDisconnected)
    {
        LOG_WARN << "disconnected, give up writing";
        return;
    }
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        // 没有在发数据
        nwrote = sockets::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_SYSERR << "TcpConnection::sendInLoop";
                if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
                {
                    faultError = true;
                }
            }
        }
    }
    assert(remaining <= len);
    if (!faultError && remaining > 0)
    {
        size_t oldlen = outputBuffer_.readableBytes();
        if (oldlen + remaining >= highWaterMark_ && oldlen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldlen + remaining));
        }
        outputBuffer_.append(static_cast<const char *>(data) + nwrote, remaining);
        if (!channel_->isWriting())
        {
            // 监听可写事件
            channel_->enableWriting();
        }
    }
}

/**
 * 作用是禁用Nagle算法，避免连续发包出现延迟，这对编写低延迟网络服务很重要
 */
void TcpConnection::setTcpNoDelay(bool on)
{
    /**
     * Nagle算法简单的讲就是，等待服务器应答包到达后，在发送下一个数据包。数据在发送端被缓存，
     * 如果缓存到达指定大小就将其发送，或者在上一个数据的应答包到达，将缓存区一次性全部发送
     * 【 若发送应用进程要把发送的数据逐个字节地送到TCP的发送缓存，
     * 则发送方就把第一个数据字节先发送出去，把后面到达的数据字节都缓存起来。
     * 当发送方收到对第一个数据字符的确认后，再把发送缓存中的所有数据组装成一个报文段发送出去，
     * 同时继续对随后到达的数据进行缓存。只有收到对前一个报文段的确认后才继续发送下一个报文段。
     * 当数据到达较快而网络速率较慢时，用这样的方法明显的减少所用的网络带宽。
     * Nagle算法还规定，当到达的数据已经达到发送窗口大小的一半或已达到报文段的最大长度时，
     * 就立即发送一个报文段。这样做，就可以有效的提高网络的吞吐量】
     */
    socket_->setTcpNoDelay(on);
}

const char *TcpConnection::stateToString() const
{
    switch (state_)
    {
    case kDisconnected:
        return "kDisconnected";
    case kConnecting:
        return "kConnecting";
    case kConnected:
        return "kConnected";
    case kDisconnecting:
        return "kDisconnecting";
    default:
        return "unknown state";
    }
}