// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "mymuduo/base/AsyncLogging.h"
#include "mymuduo/base/LogFile.h"
#include "mymuduo/base/Timestamp.h"

#include <stdio.h>

using namespace mymuduo;
/**
 *  @brief  异步日志写入本地文件
 *  @param  basename 本地文件名
 *  @param  rollSize 预留日志大小，即日志滚动阈值，当积累多条日志后将切换日志文件
 *  @param  flushInterval 刷新间隔，写入本地
 */
AsyncLogging::AsyncLogging(const string &basename,
                           off_t rollSize,
                           int flushInterval)
    : flushInterval_(flushInterval), // 刷新间隔
      running_(false),
      basename_(basename),
      rollSize_(rollSize), // 预留日志大小
      thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
      latch_(1),
      mutex_(),
      cond_(mutex_),
      currentBuffer_(new Buffer),
      nextBuffer_(new Buffer),
      buffers_()
{
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16); // 设置缓冲区大小
}

// 核心函数，所有 Log 都会调用
void AsyncLogging::append(const char *logline, int len)
{
    mymuduo::MutexLockGuard lock(mutex_);
    if (currentBuffer_->avail() > len) // 当前缓冲区还够，可以直接写
    {
        currentBuffer_->append(logline, len);
    }
    else
    {
        buffers_.push_back(std::move(currentBuffer_)); // 添加到已满缓冲区队列

        if (nextBuffer_) // 优先选择 nextBuffer_ 作为可用缓冲区
        {
            currentBuffer_ = std::move(nextBuffer_);
        }
        else
        {
            // 如果前端写入速度太快了，一下子把两块缓冲都用完了，那么只好分配一块新的buffer,作当前缓冲，这是极少发生的情况
            currentBuffer_.reset(new Buffer); // Rarely happens
        }
        currentBuffer_->append(logline, len);
        // 通知日志线程，有数据可写，当缓冲区满了之后就会提前唤醒，将数据写入日志文件中
        cond_.notify();
    }
}

void AsyncLogging::threadFunc()
{
    assert(running_ == true);
    latch_.countDown();
    // 创建日志文件
    LogFile output(basename_, rollSize_, false);
    // 除了本身的 current 和 next 缓冲区，还存了两个额外缓冲区
    // BufferPtr 属于 unique_ptr 只能使用 std::move 移动
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    newBuffer1->bzero();
    newBuffer2->bzero();
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    while (running_)
    {
        assert(newBuffer1 && newBuffer1->length() == 0);
        assert(newBuffer2 && newBuffer2->length() == 0);
        assert(buffersToWrite.empty());

        {
            mymuduo::MutexLockGuard lock(mutex_);
            if (buffers_.empty()) // unusual usage!
            {
                // 通过条件变量的超时等待，既能监听可读事件，又能实现定时刷新
                cond_.waitForSeconds(flushInterval_);
            }
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer1);
            // 使用新的未使用的 buffersToWrite 交换 buffers_，将buffers_中的数据在异步线程中写入 LogFile 中
            // 使用写时复制，将需要IO写出的Buffer交换到临时变量 buffersToWrite中，从而缩小临界区域
            buffersToWrite.swap(buffers_);
            if (!nextBuffer_)
            {
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        assert(!buffersToWrite.empty());

        // 如果从主程序获得的 buffer vector 长度大于 25，超过100MB 删除多余缓冲 有利于提升磁盘性能
        if (buffersToWrite.size() > 25)
        {
            char buf[256];
            snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
                     Timestamp::now().toFormattedString().c_str(),
                     buffersToWrite.size() - 2);
            fputs(buf, stderr);
            output.append(buf, static_cast<int>(strlen(buf)));
            // buffer vector 只留下两个 buffer 使用 std::move 移动，节省资源
            buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
        }

        for (const auto &buffer : buffersToWrite)
        {
            // FIXME: use unbuffered stdio FILE ? or use ::writev ?
            output.append(buffer->data(), buffer->length());
        }

        // 擦除多余缓冲，只用保留两个，归还给buffer1和buffer2
        if (buffersToWrite.size() > 2)
        {
            // drop non-bzero-ed buffers, avoid trashing 丢弃非零的缓冲区，避免垃圾
            buffersToWrite.resize(2);
        }

        // 当两个额外的缓冲区被使用时，就重新申请资源
        if (!newBuffer1)
        {
            assert(!buffersToWrite.empty());
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        if (!newBuffer2)
        {
            assert(!buffersToWrite.empty());
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        buffersToWrite.clear();
        // 将内核高速缓存中的数据flush到磁盘，防止意外情况造成数据丢失
        output.flush();
    }
    output.flush();
}
