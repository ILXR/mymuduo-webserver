// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "mymuduo/base/Timestamp.h"

#include <sys/time.h>
#include <stdio.h>

// 要使用 PRId64 进行跨平台输出，就必须定义 __STDC_FORMAT_MACROS 宏
// 然后再 include 头文件 inttypes.h，才能使 PRId64 生效
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>
#include <cassert>

using namespace mymuduo;

static_assert(sizeof(Timestamp) == sizeof(int64_t),
              "Timestamp should be same size as int64_t");

string Timestamp::toString() const
{
    char buf[32] = {0};
    int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
    // PRId64 定义在头文件 inttypes.h 中
    // PRId64 这是一种跨平台的书写方式，主要是为了同时支持32位和64位操作系统。
    // PRId64表示64位整数，在32位系统中表示long long int，在64位系统中表示long int
    // 分别对应 "%lld" 和 "%ld"
    snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
    return buf;
}

string Timestamp::toFormattedString(bool showMicroseconds) const
{
    char buf[64] = {0};
    time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    struct tm tm_time;
    gmtime_r(&seconds, &tm_time);

    if (showMicroseconds)
    {
        int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
                 microseconds);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }
    return buf;
}

Timestamp Timestamp::now()
{
    /** Linux 获取时间戳
     * 使用gettimeofday(2)，分辨率1us，其实现也能达到毫秒级（当然分辨率不等于精度）
     *
     * time(2)只能精确到1s，ftime(3)已被废弃，clock_gettime(2)精度高，但系统调用开销比gettimeofday(2)大，
     * 网络编程中，最适合用 gettimeofday(2)来计时
     */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;
    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}
