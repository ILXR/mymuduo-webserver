#include "mymuduo/net/Acceptor.h"

#include "mymuduo/base/Logging.h"
#include "mymuduo/net/EventLoop.h"
#include "mymuduo/net/SocketsOps.h"
#include "mymuduo/net/InetAddress.h"

#include <functional>
#include <fcntl.h>

using namespace mymuduo;
using namespace mymuduo::net;

/**
 * Acceptor的构造函数和Acceptor::listen()成员函数执行创建TCP服务端的传统步骤，
 * 即调用socket(2)、bind(2)、listen(2)等Sockets API，
 * 其中任何一个步骤出错都会造成程序终止，因此这里看不到错误处理
 */
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr)
    : listening_(false),
      loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie(AF_INET)), // ipv4
      acceptChannel_(loop_, acceptSocket_.fd()),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    assert(idleFd_ >= 0);
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    // Acceptor供TcpServer使用，调用析构函数说明整个链接已断开
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    listening_ = false;
    ::close(idleFd_);
}

void Acceptor::listen()
{
    // Acceptor封装好，只供TcpServer使用，必须在自己的线程上阻塞
    loop_->assertInLoopThread();
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
    loop_->assertInLoopThread();
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (newConnectionCallback_)
        {
            /**
             * 直接把socket fd传给callback，这种传递 int句柄的做法不够理想，
             * 在C++11中可以先创建Socket对象，再用移动语义
             * 把Socket对象std::move()给回调函数，确保资源的安全释放
             */
            newConnectionCallback_(connfd, peerAddr);
        }
        else
        {
            sockets::close(connfd);
        }
    }
    else
    {
        LOG_SYSERR << "in Acceptor::handleRead";
        // Read the section named "The special problem of
        // accept()ing when you can't" in libev's doc.
        // By Marc Lehmann, author of libev.
        if (errno == EMFILE)
        {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
