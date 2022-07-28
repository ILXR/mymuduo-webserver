#include "mymuduo/http/HttpResponse.h"

#include "mymuduo/base/Logging.h"
#include "mymuduo/net/Buffer.h"

#include <stdio.h>

using namespace mymuduo;
using namespace mymuduo::net;

void HttpResponse::appendToBuffer(Buffer *output) const
{
    char buf[32];
    snprintf(buf, sizeof buf, "HTTP/1.1 %d ", statusCode_);
    output->append(buf);
    output->append(statusMessage_);
    output->append("\r\n");

    if (closeConnection_)
    {
        // HTTP1.1中，建立的HTTP请求默认是持久连接的。当Client确定不再需要向Server发送数据时，
        // 它可以关闭连接，即在发送首部中添加Connection:Closed字段。
        output->append("Connection: close\r\n");
    }
    else
    {
        // 格式符z和整数转换说明符一起使用，表示对应数字是一个size_t值。属于C99。
        if (!needSendFile())
            snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", body_.size());
        output->append(buf);
        // HTTP/1.1 之前的 HTTP 版本的默认连接都是非持久连接。为了兼容老版本，
        // 则需要指定 Connection 首部字段的值为 Keep-Alive
        output->append("Connection: Keep-Alive\r\n");
    }

    for (const auto &header : headers_)
    {
        output->append(header.first);
        output->append(": ");
        output->append(header.second);
        output->append("\r\n");
    }

    output->append("\r\n");
    output->append(body_);
}