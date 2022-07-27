#ifndef MYMUDUO_HTTP_HTTPRESPONSE_H
#define MYMUDUO_HTTP_HTTPRESPONSE_H

#include "mymuduo/base/copyable.h"
#include "mymuduo/base/Types.h"

#include <map>

namespace mymuduo
{
    namespace net
    {
        class Buffer;

        /** Http 响应报文 （CRLF 表示回车换行  | 表示空格）
         * 状态行：版本 | 状态码 | 短语 CRLF
         * 首部行：首部字段名: | 值 CRLF
         * CRLF
         * 实体主体（有些响应报文不用）
         *
         * 封装了构造响应报文的格式化过程
         */
        class HttpResponse : public mymuduo::copyable
        {
        public:
            enum HttpStatusCode
            {
                kUnknown,
                k200Ok = 200,
                k204NoContent = 204,
                k206Partitial = 206,
                k301MovedPermanently = 301,
                k400BadRequest = 400,
                k404NotFound = 404,
                k500InternalError = 500
            };

            explicit HttpResponse(bool close) : statusCode_(kUnknown),
                                                closeConnection_(close),
                                                fd_(-1), len_(0) {}
            ~HttpResponse();

            void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
            void setStatusMessage(const string &message) { statusMessage_ = message; }
            void setCloseConnection(bool on) { closeConnection_ = on; }
            bool closeConnection() const { return closeConnection_; }
            void setContentType(const string &contentType) { addHeader("Content-Type", contentType); }
            // FIXME: replace string with StringPiece
            void addHeader(const string &key, const string &value) { headers_[key] = value; }
            void setBody(const string &body) { body_ = body; }
            void appendToBuffer(Buffer *output) const;

            bool needSendFile() const { return fd_ != -1; }
            int getFd() const { return fd_; }
            void setFd(int fd)
            {
                // assert(fd >= 0);
                fd_ = fd;
            }
            off64_t getSendLen() const { return len_; }
            void setSendLen(off64_t len)
            {
                // assert(len > 0);
                len_ = len;
            }

        private:
            std::map<string, string> headers_; // 首部字段
            HttpStatusCode statusCode_;        // 状态码
            string statusMessage_;             // 状态短语
            bool closeConnection_;             // 是否关闭长连接
            string body_;                      // 实体主体
            int fd_;                           // 需要传输文件时使用
            off64_t len_;                          // 传输大小
        };
    }
}

#endif