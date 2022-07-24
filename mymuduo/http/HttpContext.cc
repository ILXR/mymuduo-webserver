#include "mymuduo/net/Buffer.h"
#include "mymuduo/http/HttpContext.h"
#include "mymuduo/base/Exception.h"

#include <string.h>
#include <stdio.h>

using namespace mymuduo;
using namespace mymuduo::net;

/**
 *  @brief  解析请求行 [ 方法 <space> URL <space> 版本 CRLF ]
 *  @param  begin 指向当前行的第一个字符
 *  @param  end 指向当前行 "\\r\\n" 前的最后一个字符
 */
bool HttpContext::processRequestLine(const char *begin, const char *end)
{
    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    // 第一个 space 前面是请求的方法
    if (space != end && request_.setMethod(start, space))
    {
        start = space + 1;
        space = std::find(start, end, ' ');
        // 第二个 space 前面是 URL
        if (space != end)
        {
            /** 传统的 URL 格式
             * 格式：schema://host:port/path?query#fragment
             *  ① schema：协议。例如http、https、ftp等（必写）。
             * ​ ② host：域名或IP地址（必写）。
             * ​ ③ port：端口号，http 默认端口为 80 ，可以省略。
             * ​ ④ path：路径，例如/view/index。
             * ​ ⑤ query：参数，例如 uname=zhangsan&age=20，可以省略。
             * ​ ⑥ fragment：锚点（哈希 Hash），用于定位页面的某个位置，可以省略。
             */
            const char *question = std::find(start, space, '?');
            if (question != space)
            {
                request_.setPath(start, question);
                request_.setQuery(question, space);
            }
            else
            {
                request_.setPath(start, space);
            }
            start = space + 1;
            // 只能解析 HTTP 1.0/1.1
            succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
            if (succeed)
            {
                if (*(end - 1) == '1')
                {
                    request_.setVersion(HttpRequest::kHttp11);
                }
                else if (*(end - 1) == '0')
                {
                    request_.setVersion(HttpRequest::kHttp10);
                }
                else
                {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}

bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime)
{
    bool ok = true;      // ok 表示解析是否成功，只有解析格式错误才修改该值
    bool hasMore = true; // hasMore 表示是否还有待解析内容
    while (hasMore)
    {
        if (state_ == kExpectRequestLine)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    // 解析完请求行，说明这是一次 Http request，此时才计时
                    request_.setReceiveTime(receiveTime);
                    // 将缓冲区中的 readable 指针后移，相当于跳过已经解析的请求行
                    buf->retrieveUntil(crlf + 2);
                    state_ = kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                // 首部字段名: <space> 值 CRLF
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // empty line, end of header
                    state_ = kExpectBody;
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectBody)
        {
            // 通过 Content-Length 来定义主体大小，如果没有该字段则默认不存在主体
            string lenstr = request_.getHeader("Content-Length");
            if (!lenstr.empty()) // 请求定义了主体大小
            {
                try
                {
                    size_t len = static_cast<size_t>(std::stoll(lenstr));
                    if (len >= buf->readableBytes())
                    {
                        request_.setBody(buf->peek(), buf->peek() + len);
                        buf->retrieve(len);
                        state_ = kGotAll;
                        hasMore = false;
                    }
                }
                catch (Exception &ex)
                {
                    fprintf(stderr, "Http request analyze fail: Content-Length = %s", lenstr.c_str());
                    fprintf(stderr, "reason: %s\n", ex.what());
                    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
                    buf->retrieveAll();
                    ok = false;
                    hasMore = false;
                }
            }
            else
            {
                state_ = kGotAll;
                hasMore = false;
            }
        }
    }
    return ok;
}