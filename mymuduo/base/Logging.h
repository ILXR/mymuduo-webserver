// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MYMUDUO_BASE_LOGGING_H
#define MYMUDUO_BASE_LOGGING_H

#include "mymuduo/base/LogStream.h"
#include "mymuduo/base/Timestamp.h"

namespace mymuduo
{

    class TimeZone;

    // 每条日志都使用一个 Logger 来存储
    class Logger
    {
    public:
        enum LogLevel // 日志等级
        {
            TRACE, // 指出比DEBUG粒度更细的一些信息事件（开发过程中使用）
            DEBUG, // 指出细粒度信息事件对调试应用程序是非常有帮助的。（开发过程中使用）
            INFO,  // 表明消息在粗粒度级别上突出强调应用程序的运行过程。
            WARN,  // 系统能正常运行，但可能会出现潜在错误的情形。
            ERROR, // 指出虽然发生错误事件，但仍然不影响系统的继续运行。
            FATAL, // 指出每个严重的错误事件将会导致应用程序的退出。
            NUM_LOG_LEVELS,
        };

        /**
         * compile time calculation of basename of source file
         * 用于在输出信息的时候指明是在具体的出错文件，是对字符串的简单封装
         */
        class SourceFile
        {
        public:
            template <int N>
            SourceFile(const char (&arr)[N]) : data_(arr), size_(N - 1)
            {
                // C 库函数 char *strrchr(const char *str, int c) 在参数 str 所指向的字符串中
                // 搜索最后一次出现字符 c（一个无符号字符）的位置。
                const char *slash = strrchr(data_, '/'); // builtin function
                if (slash)
                {
                    data_ = slash + 1;
                    size_ -= static_cast<int>(data_ - arr);
                }
            }

            explicit SourceFile(const char *filename) : data_(filename)
            {
                const char *slash = strrchr(filename, '/');
                if (slash)
                {
                    data_ = slash + 1;
                }
                size_ = static_cast<int>(strlen(data_));
            }

            const char *data_;
            int size_;
        };

        Logger(SourceFile file, int line);
        Logger(SourceFile file, int line, LogLevel level);
        Logger(SourceFile file, int line, LogLevel level, const char *func);
        Logger(SourceFile file, int line, bool toAbort);
        ~Logger();

        LogStream &stream() { return impl_.stream_; }

        static LogLevel logLevel();              // 当前日志级别
        static void setLogLevel(LogLevel level); // 设置日志级别

        typedef void (*OutputFunc)(const char *msg, int len);
        typedef void (*FlushFunc)();
        static void setOutput(OutputFunc); // 替代默认输出函数
        static void setFlush(FlushFunc);   // 替代默认flush函数
        static void setTimeZone(const TimeZone &tz);

    private:
        // Impl类由Logger调用，输出出错信息，封装了Logger的缓冲区stream_
        class Impl
        {
        public:
            typedef Logger::LogLevel LogLevel;
            Impl(LogLevel level, int old_errno, const SourceFile &file, int line);
            // 将出错的时间格式化为年-月-日-时-分-秒-毫秒的形式，并输入到stream_的缓冲区中, 主要是利用了gmtime_r函数
            void formatTime();
            void finish();

            Timestamp time_;
            // 构造日志缓冲区，每一条信息都会通过一个Logger构造一个该类的实例
            LogStream stream_;

            LogLevel level_;
            int line_;
            SourceFile basename_;
        };

        Impl impl_;
    };

    extern Logger::LogLevel g_logLevel;

    inline Logger::LogLevel Logger::logLevel() { return g_logLevel; }

    //
    // CAUTION: do not write:
    //
    // if (good)
    //   LOG_INFO << "Good news";
    // else
    //   LOG_WARN << "Bad news";
    //
    // this expends to
    //
    // if (good)
    //   if (logging_INFO)
    //     logInfoStream << "Good news";
    //   else
    //     logWarnStream << "Bad news";
    //

    /**
     * 当我们使用 LOG_INFO << "info…" 时，
     * 相当于用 __FILE__ (文件的完整路径和文件名)与 __LINE__(当前行号)初始化了一个Logger类，
     * 获取出错时间、线程、文件、行号初始化号，接着将"info..."输入到缓冲区，在Logger生命周期结束时调用析构函数输出错误信息。
     * if (muduo::Logger::logLevel() <= muduo::Logger::TRACE)
     * 这代表我们使用该LOG_INFO宏时会先进行判断，如果级别大于INFO级别，后面那句不会被执行，也就是不会打印INFO级别的信息。
     * 整个调用过程：Logger --> impl --> LogStream --> operator<< FilxedBuffer --> g_output --> g_flush
     */

#define LOG_TRACE                                              \
    if (mymuduo::Logger::logLevel() <= mymuduo::Logger::TRACE) \
    mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::TRACE, __func__).stream()
#define LOG_DEBUG                                              \
    if (mymuduo::Logger::logLevel() <= mymuduo::Logger::DEBUG) \
    mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::DEBUG, __func__).stream()
#define LOG_INFO                                              \
    if (mymuduo::Logger::logLevel() <= mymuduo::Logger::INFO) \
    mymuduo::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::WARN).stream()
#define LOG_ERROR mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::ERROR).stream()
#define LOG_FATAL mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::FATAL).stream()
#define LOG_SYSERR mymuduo::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL mymuduo::Logger(__FILE__, __LINE__, true).stream()

    const char *strerror_tl(int savedErrno);

    // Taken from glog/logging.h
    //
    // Check that the input is non NULL.  This very useful in constructor
    // initializer lists.

#define CHECK_NOTNULL(val) \
    ::mymuduo::CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

    // A small helper for CHECK_NOTNULL().
    template <typename T>
    T *CheckNotNull(Logger::SourceFile file, int line, const char *names, T *ptr)
    {
        if (ptr == NULL)
        {
            Logger(file, line, Logger::FATAL).stream() << names;
        }
        return ptr;
    }

} // namespace mymuduo

#endif // MUDUO_BASE_LOGGING_H
