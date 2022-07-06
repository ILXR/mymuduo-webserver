// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MYMUDUO_BASE_LOGSTREAM_H
#define MYMUDUO_BASE_LOGSTREAM_H

// 实现了自定义的输出流，将各种类型的数据输出到成员变量 FixedBuffer 中
// LogStream的性能优于ostringstream和stdio（double除外，使用stdio实现）

#include "mymuduo/base/noncopyable.h"
#include "mymuduo/base/StringPiece.h"
#include "mymuduo/base/Types.h"
#include <assert.h>
#include <string.h> // memcpy

namespace mymuduo
{

    namespace detail
    {
        // FixedBuffer 模版类有两个特例化，以下分别定义缓冲区尺寸
        const int kSmallBuffer = 4000;
        const int kLargeBuffer = 4000 * 1000;

        /**
         * FixedBuffer的实现为一个非类型参数的模板类（对于非类型参数而言，
         * 目前C++的标准仅支持整型、枚举、指针类型和引用类型），传入一个非类型参数SIZE表示缓冲区的大小。
         * 通过成员 data_首地址、cur_指针、end()完成对缓冲区的各项操作。
         * 通过append()接口把日志内容添加到缓冲区来。
         */
        template <int SIZE>
        class FixedBuffer : noncopyable
        {
        public:
            FixedBuffer() : cur_(data_) { setCookie(cookieStart); }

            ~FixedBuffer() { setCookie(cookieEnd); }

            void append(const char * /*restrict*/ buf, size_t len)
            {
                // FIXME: append partially
                if (implicit_cast<size_t>(avail()) > len)
                {
                    memcpy(cur_, buf, len);
                    cur_ += len;
                }
            }

            const char *data() const { return data_; }
            int length() const { return static_cast<int>(cur_ - data_); }

            // write to data_ directly
            char *current() { return cur_; }
            int avail() const { return static_cast<int>(end() - cur_); }
            void add(size_t len) { cur_ += len; }

            void reset() { cur_ = data_; }
            void bzero() { memZero(data_, sizeof data_); }

            // for used by GDB
            const char *debugString();
            void setCookie(void (*cookie)()) { cookie_ = cookie; }
            // for used by unit test
            string toString() const { return string(data_, length()); }
            StringPiece toStringPiece() const { return StringPiece(data_, length()); }

        private:
            const char *end() const { return data_ + sizeof data_; }
            // Must be outline function for cookies.
            static void cookieStart();
            static void cookieEnd();

            void (*cookie_)();
            char data_[SIZE];
            char *cur_;
        };

    } // namespace detail

    // 为了性能考虑，代替 iostream/stringstream，重点在于运算符重载
    class LogStream : noncopyable
    {
        typedef LogStream self;

    public:
        typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;

        self &operator<<(bool v)
        {
            buffer_.append(v ? "1" : "0", 1);
            return *this;
        }

        // 整形数据的字符串转换、保存到缓冲区； 内部均调用 void formatInteger(T); 函数
        self &operator<<(short);
        self &operator<<(unsigned short);
        self &operator<<(int);
        self &operator<<(unsigned int);
        self &operator<<(long);
        self &operator<<(unsigned long);
        self &operator<<(long long);
        self &operator<<(unsigned long long);

        // 指针转换为16进制字符串
        self &operator<<(const void *);

        // 浮点类型数据转换成字符串， 内部使用 snprintf 函数
        self &operator<<(float v)
        {
            *this << static_cast<double>(v);
            return *this;
        }
        self &operator<<(double);
        // self& operator<<(long double);

        self &operator<<(char v)
        {
            buffer_.append(&v, 1);
            return *this;
        }

        // self& operator<<(signed char);
        // self& operator<<(unsigned char);

        // 原生字符串输出到缓冲区
        self &operator<<(const char *str)
        {
            if (str)
            {
                buffer_.append(str, strlen(str));
            }
            else
            {
                buffer_.append("(null)", 6);
            }
            return *this;
        }
        self &operator<<(const unsigned char *str)
        {
            return operator<<(reinterpret_cast<const char *>(str));
        }

        // 标准字符串std::string输出到缓冲区
        self &operator<<(const string &v)
        {
            buffer_.append(v.c_str(), v.size());
            return *this;
        }

        self &operator<<(const StringPiece &v)
        {
            buffer_.append(v.data(), v.size());
            return *this;
        }

        // Buffer输出到缓冲区
        self &operator<<(const Buffer &v)
        {
            *this << v.toStringPiece();
            return *this;
        }

        void append(const char *data, int len) { buffer_.append(data, len); }
        const Buffer &buffer() const { return buffer_; }
        void resetBuffer() { buffer_.reset(); }

    private:
        void staticCheck();

        // 整形数据转换字符串的模板函数
        template <typename T>
        void formatInteger(T);

        Buffer buffer_;

        static const int kMaxNumericSize = 48;
    };

    // 使 LogStream 支持格式化数据的输出
    // 本质是使用 snprintf 将格式化数据输出到Fmt中的缓冲区，然后再输出到 LogStream中
    class Fmt // : noncopyable
    {
    public:
        template <typename T>
        Fmt(const char *fmt, T val);

        const char *data() const { return buf_; }
        int length() const { return length_; }

    private:
        char buf_[32];
        int length_;
    };

    inline LogStream &operator<<(LogStream &s, const Fmt &fmt)
    {
        s.append(fmt.data(), fmt.length());
        return s;
    }

    // Format quantity n in SI units (k, M, G, T, P, E).
    // The returned string is atmost 5 characters long.
    // Requires n >= 0
    string formatSI(int64_t n);

    // Format quantity n in IEC (binary) units (Ki, Mi, Gi, Ti, Pi, Ei).
    // The returned string is atmost 6 characters long.
    // Requires n >= 0
    string formatIEC(int64_t n);

} // namespace mymuduo

#endif
