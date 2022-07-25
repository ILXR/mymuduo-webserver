#ifndef MYMUDUO_HTTP_FILESERVER_H
#define MYMUDUO_HTTP_FILESERVER_H

#include "mymuduo/net/TcpServer.h"
#include <map>

namespace mymuduo
{
    namespace net
    {
        class HttpRequest;
        class HttpResponse;

        class MimeType
        {
        private:
            static void init();
            static std::map<std::string, std::string> mime;
            MimeType();
            MimeType(const MimeType &m);

        public:
            static std::string getMime(const std::string &suffix);

        private:
            static pthread_once_t once_control;
        };

        /**
         * 模仿 python http.server 实现的本地文件服务器
         * 支持文件下载范围请求，即 range 首部字段
         * 用户只需要设置工作路径即可
         */
        class FileServer : noncopyable
        {
        public:
            FileServer(const string &path,
                       EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &name,
                       TcpServer::Option option = TcpServer::kNoReusePort);

            EventLoop *getLoop() const { return server_.getLoop(); }

            void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }
            void start();

        private:
            void onMessage(const TcpConnectionPtr &conn,
                           Buffer *buf,
                           Timestamp receiveTime);
            void onRequest(const TcpConnectionPtr &, const HttpRequest &);
            void onConnection(const TcpConnectionPtr &conn);

            string workPath_;
            TcpServer server_;
        };
    }
}

#endif