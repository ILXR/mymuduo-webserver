#include "mymuduo/net/EventLoop.h"
#include "mymuduo/net/Acceptor.h"
#include "mymuduo/net/SocketsOps.h"
#include "mymuduo/net/Socket.h"
#include "mymuduo/net/InetAddress.h"

using namespace mymuduo;
using namespace mymuduo::net;

void newConnection(int sockfd, const InetAddress &peerAddr)
{
    printf("newConnection(): accepted a new connection from %s\n", peerAddr.toIpPort().c_str());
    ssize_t len = ::write(sockfd, "How are you\n", 13);
    (void)len;
    sockets::close(sockfd);
}

int main(int argc, char const *argv[])
{
    printf("main(): pid = %d\n", getpid());

    InetAddress listenAddr(9981);
    EventLoop loop;

    Acceptor acceptor(&loop, listenAddr);
    acceptor.setNewConnectionCallback(newConnection);
    acceptor.listen();

    loop.loop();
}
