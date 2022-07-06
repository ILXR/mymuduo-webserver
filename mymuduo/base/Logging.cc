// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "mymuduo/base/Logging.h"

#include "mymuduo/base/CurrentThread.h"
#include "mymuduo/base/Timestamp.h"
#include "mymuduo/base/TimeZone.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sstream>

namespace mymuduo
{

    /*
    class LoggerImpl
    {
     public:
      typedef Logger::LogLevel LogLevel;
      LoggerImpl(LogLevel level, int old_errno, const char* file, int line);
      void finish();

      Timestamp time_;
      LogStream stream_;
      LogLevel level_;
      int line_;
      const char* fullname_;
      const char* basename_;
    };
    */

    __thread char t_errnobuf[512];
    __thread char t_time[64];
    __thread time_t t_lastSecond;

    const char *strerror_tl(int savedErrno)
    {
        return strerror_r(savedErrno, t_errnobuf, sizeof t_errnobuf);
    }

    // 通过在环境变量中定义 trace 或 debug 来设置程序的日志级别
    Logger::LogLevel initLogLevel()
    {
        if (::getenv("MUDUO_LOG_TRACE"))
            return Logger::TRACE;
        else if (::getenv("MUDUO_LOG_DEBUG"))
            return Logger::DEBUG;
        else
            return Logger::INFO;
    }

    Logger::LogLevel g_logLevel = initLogLevel();

    const char *LogLevelName[Logger::NUM_LOG_LEVELS] =
        {
            "TRACE ",
            "DEBUG ",
            "INFO  ",
            "WARN  ",
            "ERROR ",
            "FATAL ",
    };

    // helper class for known string length at compile time
    // 编译时获取字符串长度
    class T
    {
    public:
        T(const char *str, unsigned len)
            : str_(str),
              len_(len)
        {
            assert(strlen(str) == len_);
        }

        const char *str_;
        const unsigned len_;
    };

    inline LogStream &operator<<(LogStream &s, T v)
    {
        s.append(v.str_, v.len_);
        return s;
    }

    inline LogStream &operator<<(LogStream &s, const Logger::SourceFile &v)
    {
        s.append(v.data_, v.size_);
        return s;
    }

    // 默认输出到 stdout
    void defaultOutput(const char *msg, int len)
    {
        size_t n = fwrite(msg, 1, len, stdout);
        // FIXME check n
        (void)n;
    }

    void defaultFlush()
    {
        fflush(stdout);
    }

    Logger::OutputFunc g_output = defaultOutput;
    Logger::FlushFunc g_flush = defaultFlush;
    TimeZone g_logTimeZone;

} // namespace mymuduo

using namespace mymuduo;

/**
 * 构造函数中，我们用现在的时间初始化time_,line_是出错处的文件行号，basename就是出错文件的文件名。
 * 初始化结束后，执行formatTime函数，将当前线程号和LogLevel输入stream_的缓冲区。
 * saveErrno我们一般传入errno(系统的最后一次错误代码)，若不等于0，将它错误原因输入到缓冲区。
 */
Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile &file, int line)
    : time_(Timestamp::now()),
      stream_(),
      level_(level),
      line_(line),
      basename_(file)
{
    formatTime();
    CurrentThread::tid(); // 缓存当前的线程id
    // 将当前线程号和LogLevel输入stream_的缓冲区
    stream_ << T(CurrentThread::tidString(), CurrentThread::tidStringLength()); // 格式化线程tid字符串
    stream_ << T(LogLevelName[level], 6);                                       // 格式化级别，对应成字符串，先输出到缓冲区
    if (savedErrno != 0)
    {
        stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";
    }
}

void Logger::Impl::formatTime()
{
    int64_t microSecondsSinceEpoch = time_.microSecondsSinceEpoch();
    time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / Timestamp::kMicroSecondsPerSecond);
    int microseconds = static_cast<int>(microSecondsSinceEpoch % Timestamp::kMicroSecondsPerSecond);
    if (seconds != t_lastSecond)
    {
        t_lastSecond = seconds;
        struct tm tm_time;
        if (g_logTimeZone.valid())
        {
            tm_time = g_logTimeZone.toLocalTime(seconds);
        }
        else
        {
            // 将seconds转化成tm_time中保存的信息
            ::gmtime_r(&seconds, &tm_time); // FIXME TimeZone::fromUtcTime
        }

        int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
                           tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                           tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
        assert(len == 17);
        (void)len;
    }

    if (g_logTimeZone.valid())
    {
        Fmt us(".%06d ", microseconds);
        assert(us.length() == 8);
        stream_ << T(t_time, 17) << T(us.data(), 8);
    }
    else
    {
        Fmt us(".%06dZ ", microseconds);
        assert(us.length() == 9);
        stream_ << T(t_time, 17) << T(us.data(), 9);
    }
}

void Logger::Impl::finish()
{
    stream_ << " - " << basename_ << ':' << line_ << '\n';
}

Logger::Logger(SourceFile file, int line)
    : impl_(INFO, 0, file, line) {}

Logger::Logger(SourceFile file, int line, LogLevel level, const char *func)
    : impl_(level, 0, file, line)
{
    impl_.stream_ << func << ' ';
}

Logger::Logger(SourceFile file, int line, LogLevel level)
    : impl_(level, 0, file, line) {}

// toAbort 是否终止，FATAL这一日志级别会导致应用程序的退出
Logger::Logger(SourceFile file, int line, bool toAbort)
    : impl_(toAbort ? FATAL : ERROR, errno, file, line) {}

// 析构时会将名字、行数输入缓冲区，并将缓冲区内容通过设置的 g_output 函数进行写或输出操作
Logger::~Logger()
{
    impl_.finish();
    const LogStream::Buffer &buf(stream().buffer());
    g_output(buf.data(), buf.length());
    if (impl_.level_ == FATAL)
    {
        g_flush();
        abort();
    }
}

void Logger::setLogLevel(Logger::LogLevel level)
{
    g_logLevel = level;
}

void Logger::setOutput(OutputFunc out)
{
    g_output = out;
}

void Logger::setFlush(FlushFunc flush)
{
    g_flush = flush;
}

void Logger::setTimeZone(const TimeZone &tz)
{
    g_logTimeZone = tz;
}
