#ifndef MYMUDUO_HTTP_HTTPCONTEXT_H
#define MYMUDUO_HTTP_HTTPCONTEXT_H

#include "mymuduo/base/copyable.h"
#include "mymuduo/http/HttpRequest.h"

namespace mymuduo
{
    namespace net
    {
        class Buffer;

        class HttpContext : public mymuduo::copyable
        {
        public:
            enum HttpRequestParseState
            {
                kExpectRequestLine, // 正在解析请求行（初始状态）
                kExpectHeaders,     // 正在解析首部
                kExpectBody,        // 正在解析实体
                kGotAll,            // 解析完毕
            };

            HttpContext() : state_(kExpectRequestLine) {}

            // default copy-ctor, dtor and assignment are fine

            // 解析整个请求体，此时应该已经全部读取到缓存中,出错则返回 false
            bool parseRequest(Buffer *buf, Timestamp receiveTime);

            bool gotAll() const { return state_ == kGotAll; }

            void reset()
            {
                state_ = kExpectRequestLine;
                HttpRequest dummy;
                // 利用 swap 机制清空 request 存储内容
                request_.swap(dummy);
            }

            const HttpRequest &request() const { return request_; }
            HttpRequest &request() { return request_; }

        private:
            bool processRequestLine(const char *begin, const char *end);

            HttpRequestParseState state_; // 当前进度
            HttpRequest request_;         // 解析过程中的请求缓存
        };
    }
}

#endif