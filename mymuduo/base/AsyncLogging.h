// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MYMUDUO_BASE_ASYNCLOGGING_H
#define MYMUDUO_BASE_ASYNCLOGGING_H

#include "mymuduo/base/BlockingQueue.h"
#include "mymuduo/base/BoundedBlockingQueue.h"
#include "mymuduo/base/CountDownLatch.h"
#include "mymuduo/base/Mutex.h"
#include "mymuduo/base/Thread.h"
#include "mymuduo/base/LogStream.h"

#include <atomic>
#include <vector>

namespace mymuduo
{

    // 主要功能：管理后端线程，定时将日志缓冲写到磁盘，维护缓冲及缓冲队列。
    class AsyncLogging : noncopyable
    {
    public:
        AsyncLogging(const string &basename,
                     off_t rollSize,
                     int flushInterval = 3);

        ~AsyncLogging()
        {
            if (running_)
            {
                stop();
            }
        }

        void append(const char *logline, int len);

        // 线程开始
        void start()
        {
            running_ = true;
            thread_.start();
            latch_.wait();
        }

        // 线程结束
        void stop() NO_THREAD_SAFETY_ANALYSIS
        {
            /**
             * NO_THREAD_SAFETY_ANALYSIS是一种函数或方法的属性，它意味着对该函数关闭线程安全分析。
             * 它为以下两种函数的实现提供了可能，第一，故意设计的线程不安全的代码，
             * 第二，代码是线程安全的，但是对于线程安全分析模块来说太复杂，模块无法理解。
             */
            running_ = false;
            cond_.notify();
            thread_.join();
        }

    private:
        // 线程执行函数
        void threadFunc();

        typedef mymuduo::detail::FixedBuffer<mymuduo::detail::kLargeBuffer> Buffer;
        typedef std::vector<std::unique_ptr<Buffer>> BufferVector;
        typedef BufferVector::value_type BufferPtr;

        const int flushInterval_;   // 刷新间隔
        std::atomic<bool> running_; // 原子类型
        const string basename_;     // 日志文件
        const off_t rollSize_;      // 滚动最大值
        mymuduo::Thread thread_;
        mymuduo::CountDownLatch latch_;
        mymuduo::MutexLock mutex_; // 用来锁所有的缓冲，同时只能有一个线程在读或写
        mymuduo::Condition cond_ GUARDED_BY(mutex_);
        BufferPtr currentBuffer_ GUARDED_BY(mutex_); // 当前正在使用的缓冲区
        BufferPtr nextBuffer_ GUARDED_BY(mutex_);    // 空闲缓冲
        BufferVector buffers_ GUARDED_BY(mutex_);    // 已满缓冲队列，用于写到文件
    };

} // namespace mymuduo

#endif
