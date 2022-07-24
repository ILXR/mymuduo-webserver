#ifndef MYMUDUO_HTTP_HTTPREQUEST_H
#define MYMUDUO_HTTP_HTTPREQUEST_H

#define LIKELY(x) __builtin_expect(!!(x), 1)   // x很可能为真
#define UNLIKELY(x) __builtin_expect(!!(x), 0) // x很可能为假

#include "mymuduo/base/copyable.h"
#include "mymuduo/base/Timestamp.h"
#include "mymuduo/base/Types.h"
#include "mymuduo/base/Mutex.h"

#include <map>
#include <assert.h>
#include <stdio.h>

// ┌────────────────────────────────────────────────────────────────────────────────────────────────┐
// │                                              href                                              │
// ├──────────┬──┬─────────────────────┬────────────────────────┬───────────────────────────┬───────┤
// │ protocol │  │        auth         │          host          │           path            │ hash  │
// │          │  │                     ├─────────────────┬──────┼──────────┬────────────────┤       │
// │          │  │                     │    hostname     │ port │ pathname │     search     │       │
// │          │  │                     │                 │      │          ├─┬──────────────┤       │
// │          │  │                     │                 │      │          │ │    query     │       │
// "  https:   //    user   :   pass   @ sub.example.com : 8080   /p/a/t/h  ?  query=string   #hash "
// │          │  │          │          │    hostname     │ port │          │                │       │
// │          │  │          │          ├─────────────────┴──────┤          │                │       │
// │ protocol │  │ username │ password │          host          │          │                │       │
// ├──────────┴──┼──────────┴──────────┼────────────────────────┤          │                │       │
// │   origin    │                     │         origin         │ pathname │     search     │ hash  │
// ├─────────────┴─────────────────────┴────────────────────────┴──────────┴────────────────┴───────┤
// │                                              href                                              │
// └────────────────────────────────────────────────────────────────────────────────────────────────┘
// (All spaces in the "" line should be ignored. They are purely for formatting.)

namespace mymuduo
{
    namespace net
    {
        /** Http 请求报文格式（CRLF表示回车换行 |表示空格）
         * 请求行：方法 | URL | 版本 CRLF
         * 首部行：首部字段名: | 值 CRLF
         * CRLF
         * 实体主体（通常不用）
         */
        class HttpRequest : public mymuduo::copyable
        {
        public:
            enum Method
            {
                kInvalid,
                kGet,
                kPost,
                kHead,
                kPut,
                kDelete,
                // kConnect,
                // kOptions,
                // kTrace,
                // kPatch
            };

            // 只实现了 /1.0 /1.1，Http/2 是基于 HTTPS 实现的
            enum Version
            {
                kUnknown,
                kHttp10,
                kHttp11
            };

            HttpRequest() : method_(kInvalid),
                            version_(kUnknown) {}

            void setVersion(Version v) { version_ = v; }
            Version getVersion() const { return version_; }

            bool setMethod(const char *start, const char *end)
            {
                static std::map<string, Method> str2int;
                static MutexLock init;
                if (UNLIKELY(str2int.empty()))
                {
                    MutexLockGuard lock(init);
                    if (str2int.empty())
                    {
                        str2int = {
                            {"GET", kGet},
                            {"POST", kPost},
                            {"HEAD", kHead},
                            {"PUT", kPut},
                            {"DELETE", kDelete}};
                    }
                }

                assert(method_ == kInvalid);
                string m(start, end);
                auto ite = str2int.find(m);
                method_ = ite == str2int.end() ? kInvalid : ite->second;
                return method_ != kInvalid;
            }

            Method method() const { return method_; }

            const char *methodString() const
            {
                static std::map<Method, string> int2str;
                static MutexLock init;
                if (UNLIKELY(int2str.empty()))
                {
                    MutexLockGuard lock(init);
                    if (int2str.empty())
                    {
                        int2str = {
                            {kGet, "GET"},
                            {kPost, "POST"},
                            {kHead, "HEAD"},
                            {kPut, "PUT"},
                            {kDelete, "DELETE"}};
                    }
                }

                auto ite = int2str.find(method_);
                return ite == int2str.end() ? "UNKNOWN" : ite->second.c_str();
            }

            void setPath(const char *start, const char *end) { path_.assign(start, end); }
            const string &path() const { return path_; }

            void setQuery(const char *start, const char *end) { query_.assign(start, end); }
            const string &query() const { return query_; }

            void setReceiveTime(Timestamp t) { receiveTime_ = t; }
            Timestamp receiveTime() const { return receiveTime_; }

            /**
             *  @brief  解析一个首部行
             *  @param  start 指向首部行开始
             *  @param  colon 指向第一个空格
             *  @param  end 指向 CRLF("\\r\\n") 左侧
             */
            void addHeader(const char *start, const char *colon, const char *end)
            {
                string field(start, colon);
                ++colon;
                // 去掉 key - value 中间空格
                while (colon < end && isspace(*colon))
                {
                    ++colon;
                }
                string value(colon, end);
                // 去掉 value 尾部空格
                while (!value.empty() && isspace(value[value.size() - 1]))
                {
                    value.resize(value.size() - 1);
                }
                headers_[field] = value;
            }

            string getHeader(const string &field) const
            {
                string result;
                std::map<string, string>::const_iterator it = headers_.find(field);
                if (it != headers_.end())
                {
                    result = it->second;
                }
                return result;
            }

            const std::map<string, string> &headers() const { return headers_; }

            void setBody(const char *start, const char *end) { body_.assign(start, end); }
            const string &getBody() const { return body_; }

            void swap(HttpRequest &that)
            {
                std::swap(method_, that.method_);
                std::swap(version_, that.version_);
                path_.swap(that.path_);
                query_.swap(that.query_);
                receiveTime_.swap(that.receiveTime_);
                headers_.swap(that.headers_);
            }

        private:
            Method method_;                    // 请求方法
            Version version_;                  // HTTP 版本
            string path_;                      // URL 中的请求路径
            string query_;                     // URL 中携带的参数
            string body_;                      // 请求主体，一般只有 POST 才有
            Timestamp receiveTime_;            // 接收请求的时间戳
            std::map<string, string> headers_; // 请求头部
        };
    }
}

#endif