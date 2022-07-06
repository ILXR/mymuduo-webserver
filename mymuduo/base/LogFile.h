// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MYMUDUO_BASE_LOGFILE_H
#define MYMUDUO_BASE_LOGFILE_H

#include "mymuduo/base/Mutex.h"
#include "mymuduo/base/Types.h"

#include <memory>

namespace mymuduo
{

    namespace FileUtil
    {
        class AppendFile;
    }

    // 提供对日志文件的操作，包括滚动日志文件、将log数据写到当前log文件、flush log数据到当前log文件。
    class LogFile : noncopyable
    {
    public:
        LogFile(const string &basename,
                off_t rollSize,
                bool threadSafe = true,
                int flushInterval = 3,
                int checkEveryN = 1024);
        ~LogFile();

        void append(const char *logline, int len); // 添加数据到缓冲区
        void flush();                              // 刷新数据到磁盘并且清空缓冲区
        bool rollFile();                           // 滚动日志文件，也就是关闭当前文件并打开新文件

    private:
        void append_unlocked(const char *logline, int len);

        static string getLogFileName(const string &basename, time_t *now);

        const string basename_;
        const off_t rollSize_;
        const int flushInterval_;
        const int checkEveryN_;

        int count_;

        std::unique_ptr<MutexLock> mutex_;
        time_t startOfPeriod_;
        time_t lastRoll_;
        time_t lastFlush_;
        std::unique_ptr<FileUtil::AppendFile> file_;

        const static int kRollPerSeconds_ = 60 * 60 * 24;
    };

} // namespace mymuduo
#endif
