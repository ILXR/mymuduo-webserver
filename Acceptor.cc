#include "Socket.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "SocketsOps.h"
#include "InetAddress.h"

#include <functional>

using namespace mymuduo;
using namespace mymuduo::net;

/**
 * Acceptor的构造函数和Acceptor::listen()成员函数执行创建TCP服务端的传统步骤，
 * 即调用socket(2)、bind(2)、listen(2)等Sockets API，
 * 其中任何一个步骤出错都会造成程序终止，因此这里看不到错误处理
 */
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie(AF_INET)), // ipv4
      acceptChannel_(loop_, acceptSocket_.fd()),
      listening_(false)
{
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
}

void Acceptor::listen()
{
    loop_->assertInLoopThread();
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
    loop_->assertInLoopThread();
    InetAddress peerAddr(0);
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
        perror("in Acceptor::handleRead\n");
    }
}
