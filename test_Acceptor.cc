#include "EventLoop.h"
#include "Acceptor.h"
#include "SocketsOps.h"
#include "Socket.h"
#include "InetAddress.h"

using namespace mymuduo;
using namespace mymuduo::net;

void newConnection(int sockfd, const InetAddress &peerAddr)
{
    printf("newConnection(): accepted a new connection from %s\n", peerAddr.toIpPort().c_str());
    ::write(sockfd, "How are you\n", 13);
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
