#include "mymuduo/net/Channel.h"
#include "mymuduo/net/Connector.h"
#include "mymuduo/net/EventLoop.h"
#include "mymuduo/net/SocketsOps.h"
#include "mymuduo/net/InetAddress.h"

using namespace mymuduo;
using namespace mymuduo::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop *loop, const InetAddress &serverAddr)
    : loop_(loop),
      serverAddr_(serverAddr),
      connect_(false),
      state_(kDisconnected),
      retryDelayMs_(kInitRetryDelayMs)
{
}

Connector::~Connector()
{
    assert(!channel_);
    // 即使该定时器已经失效也没关系，TimerQueue会处理
    loop_->cancel(timerId_);
}

void Connector::start()
{
    connect_ = true;
    loop_->runInLoop(std::bind(&Connector::startInLoop, this)); // FIXME: unsafe
}

void Connector::startInLoop()
{
    loop_->assertInLoopThread();
    assert(state_ == kDisconnected);
    if (connect_)
    {
        connect();
    }
    else
    {
        printf("do not connect");
    }
}

void Connector::stop()
{
    connect_ = false;
    loop_->queueInLoop(std::bind(&Connector::stopInLoop, this)); // FIXME: unsafe
    loop_->cancel(timerId_);                                     // FIXME: cancel timer
}

void Connector::stopInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == kConnecting)
    {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::connect()
{
    /**
     * socket是一次性的，一旦出错（比如对方拒绝连接），就无法恢复，只能关闭重来。
     * 但Connector是可以反复使用的，因此每次尝试连接都要使用新的socket文件描述符
     * 和新的Channel对象。要留意Channel对 象的生命期管理，并防止socket文件描述符泄漏
     */
    int sockfd = sockets::createNonblockingOrDie(serverAddr_.family());
    int ret = sockets::connect(sockfd, serverAddr_.getSockAddr());
    int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno)
    {
    case 0:
    case EINPROGRESS: // “正在连接”的返回码是EINPROGRESS。
    case EINTR:
    case EISCONN:
        connecting(sockfd);
        break;
        /**
         * 错误代码与 accept 不同，EAGAIN 是真的错误，表明本机 ephemeral port 暂时用完，
         * 要关闭 socket 再延期重试。
         * 另外，即便出现socket可写，也不一定意味着连接已成功建立，
         * 还需要用 getsockopt(sockfd, SOL_SOCKET, SO_ERROR, ...)再次确认一下
         */
    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
        retry(sockfd);
        break;
    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
        perror("connect error in Connector::startInLoop ");
        sockets::close(sockfd);
        break;
    default:
        perror("Unexpected error in Connector::startInLoop ");
        sockets::close(sockfd);
        // connectErrorCallback_();
        break;
    }
}

void Connector::restart()
{
    loop_->assertInLoopThread();
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

void Connector::connecting(int sockfd)
{
    setState(kConnecting);
    assert(!channel_);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback(
        std::bind(&Connector::handleWrite, this)); // FIXME: unsafe
    channel_->setErrorCallback(
        std::bind(&Connector::handleError, this)); // FIXME: unsafe

    // channel_->tie(shared_from_this()); is not working,
    // as channel_ is not managed by shared_ptr
    channel_->enableWriting();
}

// 返回旧的 channel->fd() 并重新设置channel
int Connector::removeAndResetChannel()
{
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->fd();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    loop_->queueInLoop(std::bind(&Connector::resetChannel, this)); // FIXME: unsafe
    return sockfd;
}

void Connector::resetChannel()
{
    channel_.reset();
}

void Connector::handleWrite()
{
    printf("Connector::handleWrite \n");

    if (state_ == kConnecting)
    {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        if (err)
        {
            printf("Connector::handleWrite - SO_ERROR = %d %s\n", err, strerror(err));
            retry(sockfd);
        }
        /**
         * 要处理自连接（self-connection）。出现这种状况的原因如下。在发起连接的时候，
         * TCP/IP协议栈会先选择sourceIP和sourceport，在没有显式调用bind(2)的情况下，
         * source IP由路由表确定，source port由TCP/IP 协议栈从local port range8中
         * 选取尚未使用的port（即ephemeral port）。 如果destination IP正好是本机，
         * 而destination port位于local port range，且没有服务程序监听的话，
         * ephemeral port可能正好选中了destination port，这就出现
         *  (source IP, source port)＝(destination IP, destination port)
         * 的情况，即发生了自连接。处理办法是断开连接再重试，否则原本侦听
         * destination port的服务进程也无法启动了
         */
        else if (sockets::isSelfConnect(sockfd))
        {
            printf("Connector::handleWrite - Self connect");
            retry(sockfd);
        }
        else
        {
            // 如果读到了 错误/自连接 之外的信息，则说明已经连接成功
            setState(kConnected);
            if (connect_)
            {
                newConnectionCallback_(sockfd);
            }
            else
            {
                sockets::close(sockfd);
            }
        }
    }
    else
    {
        // what happened?
        assert(state_ == kDisconnected);
    }
}

void Connector::handleError()
{
    printf("Connector::handleError state=%d\n", state_);
    if (state_ == kConnecting)
    {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        printf("SO_ERROR = %d %s\n", err, strerror(err));
        retry(sockfd);
    }
}

void Connector::retry(int sockfd)
{
    sockets::close(sockfd);
    setState(kDisconnected);
    if (connect_)
    {
        printf("Connector::retry - Retry connecting to %s in %d milliseconds.\n",
               serverAddr_.toIpPort().c_str(), retryDelayMs_);
        /**
         * 重试的间隔应该逐渐延长，例如0.5s、1s、2s、4s，直至30s，即 back-off。
         * 这会造成对象生命期管理方面的困难，如果使用 EventLoop::runAfter()定时
         * 而Connector在定时器到期之前析构了怎么办？
         * 这里的做法是在Connector的析构函数中注销定时器
         */
        timerId_ = loop_->runAfter(retryDelayMs_ / 1000.0,
                                   std::bind(&Connector::startInLoop, shared_from_this()));
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    }
    else
    {
        perror("do not connect\n");
    }
}
