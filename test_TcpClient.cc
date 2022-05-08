#include "TcpClient.h"
#include "TcpServer.h"
#include "EventLoop.h"
#include "EventLoopThread.h"
#include "Callback.h"

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

void sendTimer(const TcpConnectionPtr &conn)
{
    if (conn->connected())
        conn->send("test");
}

void clientOnConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        printf("timer !!!!!!!!!!!!!!!!!!!!!!\n");
        conn->getLoop()->runEvery(1, std::bind(sendTimer, conn));
    }
}

void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp receiveTime)
{
    string msg = buf->retrieveAllAsString();
    printf("%s on message: %s\n", conn->name().c_str(), msg.c_str());
}

int main(int argc, char const *argv[])
{
    InetAddress serverAddr(6666);

    // 主线程loop
    EventLoop loop;

    EventLoopThread serverThread;
    EventLoop *sloop = serverThread.startLoop();
    TcpServer server(sloop, serverAddr, "TcpServer");
    server.setThreadNum(3);
    server.setMessageCallback(onMessage);
    server.setConnectionCallback(onConnection);
    server.start();

    EventLoopThread clientThread;
    EventLoop *cloop = clientThread.startLoop();
    TcpClient client(cloop, serverAddr, "client");
    client.setConnectionCallback(clientOnConnection);
    client.setMessageCallback(onMessage);
    client.connect();

    loop.loop();
}
