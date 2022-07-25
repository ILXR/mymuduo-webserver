#include "mymuduo/http/FileServer.h"
#include "mymuduo/http/HttpRequest.h"
#include "mymuduo/net/EventLoop.h"
#include "mymuduo/base/Logging.h"

using namespace mymuduo;
using namespace mymuduo::net;

int main(int argc, char *argv[])
{
    int numThreads = 0;
    if (argc > 1)
    {
        Logger::setLogLevel(Logger::WARN);
        numThreads = atoi(argv[1]);
    }
    EventLoop loop;
    FileServer server("/", &loop, InetAddress(8888), "FileServer");
    server.setThreadNum(numThreads);
    server.start();
    loop.loop();
}