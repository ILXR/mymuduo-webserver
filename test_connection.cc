#include "TcpConnection.h"
#include "EventLoop.h"
#include "TcpServer.h"

using namespace mymuduo;

void onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        printf("onConnection: new connection [%s] from %s\n",
               conn->name().c_str(),
               conn->peerAddr().toIpPort().c_str());
    }
    else
    {
        printf("onConnection(): connection [%s] is down\n",
               conn->name().c_str());
    }
}

void onMessage(const TcpConnectionPtr &conn, const char *data, ssize_t len)
{
    printf("on message\n");
}

int main(int argc, char const *argv[])
{
    printf("main(): pid = %d", getpid());

    InetAddress listenAddr(9981);
    EventLoop loop;
    TcpServer server(&loop, listenAddr);
    server.setConnectionCallback(onConnection);
    server.setMessageCallback(onMessage);
    server.start();

    return 0;
}
