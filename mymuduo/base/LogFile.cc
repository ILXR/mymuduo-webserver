// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "mymuduo/base/LogFile.h"

#include "mymuduo/base/FileUtil.h"
#include "mymuduo/base/ProcessInfo.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>

using namespace mymuduo;

LogFile::LogFile(const string &basename, //  日志文件名，默认保存在当前工作目录下
                 off_t rollSize,         //  日志文件超过设定值进行roll
                 bool threadSafe,        //  默认线程安全，使用互斥锁操作将消息写入缓冲区
                 int flushInterval,      //  flush刷新时间间隔
                 int checkEveryN)        //  每1024次日志操作，检查一个是否刷新、是否roll
    : basename_(basename),
      rollSize_(rollSize),
      flushInterval_(flushInterval),
      checkEveryN_(checkEveryN),
      count_(0),
      mutex_(threadSafe ? new MutexLock : NULL), // 操作AppendFiles是否加锁
      startOfPeriod_(0),                         // 用于标记同一天的时间戳(GMT的零点)
      lastRoll_(0),                              // 上一次roll的时间戳
      lastFlush_(0)                              // 上一次flush的时间戳
{
    assert(basename.find('/') == string::npos);
    rollFile();
}

LogFile::~LogFile() = default;

void LogFile::append(const char *logline, int len)
{
    if (mutex_)
    {
        MutexLockGuard lock(*mutex_);
        append_unlocked(logline, len);
    }
    else
    {
        append_unlocked(logline, len);
    }
}

void LogFile::flush()
{
    if (mutex_)
    {
        MutexLockGuard lock(*mutex_);
        file_->flush();
    }
    else
    {
        file_->flush();
    }
}

void LogFile::append_unlocked(const char *logline, int len)
{
    file_->append(logline, len);

    // 当前写入日志总长度超过 rollSize_， 就进行日志roll
    if (file_->writtenBytes() > rollSize_)
    {
        rollFile();
    }
    else
    {
        ++count_;
        // 每写入日志checkEveryN_ = 1024 次就检查是否需要roll
        if (count_ >= checkEveryN_)
        {
            count_ = 0;
            time_t now = ::time(NULL);
            // 将秒数对其到当天0点，如果不是同一天就会roll
            time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
            // 超过当天时间就roll
            if (thisPeriod_ != startOfPeriod_)
            {
                rollFile();
            }
            // 距离上一次flush间隔超过flushInterval_ = 3 秒就再一次flush
            else if (now - lastFlush_ > flushInterval_)
            {
                lastFlush_ = now;
                file_->flush();
            }
        }
    }
}

/**
 * 滚动日志,相当于重新生成日志文件，再向里面写数据
 * 滚动的条件如下：
 *      当前写入日志总长度超过 rollSize_
 *      每写入日志checkEveryN_ = 1024 次就检查一次，如果已经是第二天就会roll
 */
bool LogFile::rollFile()
{
    time_t now = 0;
    string filename = getLogFileName(basename_, &now);
    // 对齐值KRollPerSeconds的整数倍，也就是事件调整到当天零点(/除法会引发取整)
    time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

    if (now > lastRoll_)
    {
        lastRoll_ = now;
        lastFlush_ = now;
        startOfPeriod_ = start;
        file_.reset(new FileUtil::AppendFile(filename));
        return true;
    }
    return false;
}

// 构造日志文件名：基本名字+时间戳+主机名+进程id+“.log”后缀
string LogFile::getLogFileName(const string &basename, time_t *now)
{
    string filename;
    filename.reserve(basename.size() + 64);
    filename = basename;

    char timebuf[32];
    struct tm tm;
    *now = time(NULL);
    gmtime_r(now, &tm); // FIXME: localtime_r ?
    strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
    filename += timebuf;

    filename += ProcessInfo::hostname();

    char pidbuf[32];
    snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());
    filename += pidbuf;

    filename += ".log";

    return filename;
}
