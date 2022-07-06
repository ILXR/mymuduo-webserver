// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MYMUDUO_BASE_TIMEZONE_H
#define MYMUDUO_BASE_TIMEZONE_H

#include "mymuduo/base/copyable.h"
#include <memory>
#include <time.h>

namespace mymuduo
{
    // TimeZone for 1970~2030 时区与夏令时,方便时区之间的转换，以及时令之间的转换
    class TimeZone : public mymuduo::copyable
    {
    public:
        // zonefile，应该是保存时区信息的一个文件的地址，通过读取该文件中的信息得出时区,暂未用到
        explicit TimeZone(const char *zonefile);
        TimeZone(int eastOfUtc, const char *tzname); // a fixed timezone
        TimeZone() = default;                        // an invalid timezone

        // default copy ctor/assignment/dtor are Okay.

        bool valid() const
        {
            // 'explicit operator bool() const' in C++11
            // return static_cast<bool>(data_);
            return bool(data_);
        }

        // #include <time.h>
        // struct tm
        // {
        //     int tm_sec;   /* 秒 – 取值区间为[0,59] */
        //     int tm_min;   /* 分 - 取值区间为[0,59] */
        //     int tm_hour;  /* 时 - 取值区间为[0,23] */
        //     int tm_mday;  /* 一个月中的日期 - 取值区间为[1,31] */
        //     int tm_mon;   /* 月份（从一月开始，0代表一月） 取值区间为[0,11] */
        //     int tm_year;  /* 年份，其值等于实际年份减去1900 */
        //     int tm_wday;  /* 星期 – 取值区间为[0,6]，其中0代表星期天，1代表星期一，以此类推 */
        //     int tm_yday;  /* 从每年的1月1日开始的天数 – 取值区间为[0,365]，其中0代表1月1日，1代表1月2日，以此类推 */
        //     int tm_isdst; /* 夏令时标识符，实行夏令时的时候，tm_isdst为正。不实行夏令时的进候，tm_isdst为0；不了解情况时，tm_isdst()为负。*/
        // };

        struct tm toLocalTime(time_t secondsSinceEpoch) const;
        time_t fromLocalTime(const struct tm &) const;

        // gmtime(3)
        static struct tm toUtcTime(time_t secondsSinceEpoch, bool yday = false);
        // timegm(3)
        static time_t fromUtcTime(const struct tm &);
        // year in [1900..2500], month in [1..12], day in [1..31]
        static time_t fromUtcTime(int year, int month, int day,
                                  int hour, int minute, int seconds);

        // 真正存储数据的结构体
        struct Data;

    private:
        std::shared_ptr<Data> data_;
    };

} // namespace mymuduo

#endif // MYMUDUO_BASE_TIMEZONE_H
