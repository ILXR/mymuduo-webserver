// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "mymuduo/base/Date.h"
#include <stdio.h> // snprintf
#include <time.h>  // struct tm

namespace mymuduo
{
    namespace detail
    {

        char require_32_bit_integer_at_least[sizeof(int) >= sizeof(int32_t) ? 1 : -1];

        // algorithm and explanation see:
        // http://www.faqs.org/faqs/calendars/faq/part2/
        // http://blog.csdn.net/Solstice

        int getJulianDayNumber(int year, int month, int day)
        {
            (void)require_32_bit_integer_at_least; // no warning please
            int a = (14 - month) / 12;             // a = month<3 ? 1 : 0
            int y = year + 4800 - a;               // y = month<3 ? year+4799 : year+4800
            int m = month + 12 * a - 3;            // m = month<3 ? month+9 : month-3
            /**
             * 从 0-0-0 算起总天数，减去 julian 日期的天数
             * day 本月天数
             * (153 * m + 2) / 5 今年之前月份的天数
             * y * 365 + y / 4 - y / 100 + y / 400
             *      每年基准 365，每四年闰年加一天
             * 32045 julian 起始日期的天数
             */
            return day + (153 * m + 2) / 5 + y * 365 + y / 4 - y / 100 + y / 400 - 32045;
        }

        struct Date::YearMonthDay getYearMonthDay(int julianDayNumber)
        {
            int a = julianDayNumber + 32044;
            int b = (4 * a + 3) / 146097;
            int c = a - ((b * 146097) / 4);
            int d = (4 * c + 3) / 1461;
            int e = c - ((1461 * d) / 4);
            int m = (5 * e + 2) / 153;
            Date::YearMonthDay ymd;
            ymd.day = e - ((153 * m + 2) / 5) + 1;
            ymd.month = m + 3 - 12 * (m / 10);
            ymd.year = b * 100 + d - 4800 + (m / 10);
            return ymd;
        }
    } // namespace detail
    const int Date::kJulianDayOf1970_01_01 = detail::getJulianDayNumber(1970, 1, 1);
} // namespace muduo

using namespace mymuduo;
using namespace mymuduo::detail;

Date::Date(int y, int m, int d)
    : julianDayNumber_(getJulianDayNumber(y, m, d)) {}

/**
 * 分解时间结构体，可以表示日历时间；
 * 用time_t表示的时间（日历时间）是从一个时间点（例如：1970年1月1日0时0分0秒）到此时的秒数；
 * 用tm 则是从一个时间点到现在为止的日历时间
 * 可以使用的函数是gmtime()和localtime()将time()获得的日历时间time_t结构体转换成tm结构体。
 * 其中gmtime()函数是将日历时间转化为世界标准时间（即格林尼治时间），并返回一个tm结构体来保存这个时间，
 * 而localtime()函数是将日历时间转化为本地时间。
 */
Date::Date(const struct tm &t)
    : julianDayNumber_(getJulianDayNumber(
          t.tm_year + 1900,
          t.tm_mon + 1,
          t.tm_mday)) {}

string Date::toIsoString() const
{
    char buf[32];
    YearMonthDay ymd(yearMonthDay());
    snprintf(buf, sizeof buf, "%4d-%02d-%02d", ymd.year, ymd.month, ymd.day);
    return buf;
}

Date::YearMonthDay Date::yearMonthDay() const
{
    return getYearMonthDay(julianDayNumber_);
}
