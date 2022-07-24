#ifndef MYMUDUO_HTTP_HTTPSERVER_H
#define MYMUDUO_HTTP_HTTPSERVER_H

#include "mymuduo/net/TcpServer.h"

namespace mymuduo
{
    namespace net
    {
        class HttpRequest;
        class HttpResponse;

        class HttpServer : noncopyable
        {
        public:
            typedef std::function<void(const HttpRequest &, HttpResponse *)> HttpCallback;

            /**
             * Http 协议本质上还是建立 Tcp 之后按照特定的格式收发数据
             * 因此需要绑定到一个 EventLoop 上，而中间的很多细节可以直接复用 TcpServer 来实现
             */
            HttpServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &name,
                       TcpServer::Option option = TcpServer::kNoReusePort);

            EventLoop *getLoop() const { return server_.getLoop(); }

            /// Not thread safe, callback be registered before calling start().
            void setHttpCallback(const HttpCallback &cb) { httpCallback_ = cb; }

            void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }

            /**
             * 核心函数，开启 Tcp listen
             */
            void start();

        private:
            /**
             * Http Server 自动处理了数据收发的流程，Tcp 连接上的原始数据不会暴露给用户
             */
            void onMessage(const TcpConnectionPtr &conn,
                           Buffer *buf,
                           Timestamp receiveTime);
            void onRequest(const TcpConnectionPtr &, const HttpRequest &);
            void onConnection(const TcpConnectionPtr &conn);

            TcpServer server_;
            HttpCallback httpCallback_;
        };
    }
}

#endif