// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MY_MUDUO_BASE_TIMESTAMP_H
#define MY_MUDUO_BASE_TIMESTAMP_H

#include "mymuduo/base/copyable.h"
#include "mymuduo/base/Types.h"
#include <boost/operators.hpp>

namespace mymuduo
{

    ///
    /// Time stamp in UTC, in microseconds resolution. 使用时间戳来表示，精度为 1 微秒
    ///
    /// This class is immutable.
    /// It's recommended to pass it by value, since it's passed in register on x64.
    ///
    class Timestamp : public copyable,
                      // 只要==操作符重载，自动实现!=操作符重载
                      public boost::equality_comparable<Timestamp>,
                      // 只要实现<操作符重载，自动实现>，<=，>=操作符重载
                      public boost::less_than_comparable<Timestamp>
    {
    public:
        // Constucts an invalid Timestamp. 时间戳为 0 表示无效值
        Timestamp() : microSecondsSinceEpoch_(0) {}

        ///
        /// Constucts a Timestamp at specific time.
        /// explicit 关键字用来修饰类的构造函数，被修饰的构造函数的类，不能发生相应的隐式类型转换
        /// @param microSecondsSinceEpoch int64_t 可以表示290余年的微秒数，而32位连一年都表示不了
        explicit Timestamp(int64_t microSecondsSinceEpochArg)
            : microSecondsSinceEpoch_(microSecondsSinceEpochArg) {}

        void swap(Timestamp &that) { std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_); }

        // default copy/assignment/dtor are Okay

        // 返回时间的字符串形式，例如1649224501.687051
        string toString() const;
        /**
         * 返回时间的字符串形式，showMicroseconds为true时，例如20220406 05:55:01.687051，
         * 为false时，例如20220406 05:55:01
         */
        string toFormattedString(bool showMicroseconds = true) const;

        bool valid() const { return microSecondsSinceEpoch_ > 0; }

        // for internal usage.
        int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
        // 返回距离1970-1-1 00:00:00的秒数
        time_t secondsSinceEpoch() const
        {
            return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
        }

        ///
        /// Get time of now.
        ///
        static Timestamp now();
        // invalid 来获取一个无效的 Timestamp 对象
        static Timestamp invalid() { return Timestamp(); }

        static Timestamp fromUnixTime(time_t t) { return fromUnixTime(t, 0); }

        static Timestamp fromUnixTime(time_t t, int microseconds)
        {
            // time_t（long） 是 Unix时间戳，在头文件<time.h>，用来存储1970年以来的秒数
            return Timestamp(static_cast<int64_t>(t) * kMicroSecondsPerSecond + microseconds);
        }

        static const int kMicroSecondsPerSecond = 1000 * 1000;

    private:
        int64_t microSecondsSinceEpoch_;
    };

    inline bool operator<(Timestamp lhs, Timestamp rhs)
    {
        return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
    }

    inline bool operator==(Timestamp lhs, Timestamp rhs)
    {
        return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
    }

    ///
    /// Gets time difference of two timestamps, result in seconds.
    /// 获取时间间隔，单位为秒
    /// @param high
    /// @param low
    /// @return (high-low) in seconds
    /// @c double has 52-bit precision, enough for one-microsecond
    /// resolution for next 100 years.
    inline double timeDifference(Timestamp high, Timestamp low)
    {
        int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
        return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
    }

    ///
    /// Add @c seconds to given timestamp.
    /// 给当前时间加上对应秒数
    /// @return timestamp+seconds as Timestamp
    ///
    inline Timestamp addTime(Timestamp timestamp, double seconds)
    {
        int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
        return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
    }

} // namespace muduo

#endif // MUDUO_BASE_TIMESTAMP_H
