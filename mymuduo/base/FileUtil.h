// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MYMUDUO_BASE_FILEUTIL_H
#define MYMUDUO_BASE_FILEUTIL_H

#include "mymuduo/base/noncopyable.h"
#include "mymuduo/base/StringPiece.h"
#include <sys/types.h> // for off_t

namespace mymuduo
{
    // FileUtil用来操作文件
    namespace FileUtil
    {

        // read small file < 64KB  使用open函数用来读取小文件，主要用于 ProcessInfo 读取系统信息
        class ReadSmallFile : noncopyable
        {
        public:
            ReadSmallFile(StringArg filename);
            ~ReadSmallFile();

            // return errno
            template <typename String>
            int readToString(int maxSize,          //最大的长度
                             String *content,      //读入content缓冲区
                             int64_t *fileSize,    //文件大小
                             int64_t *modifyTime,  //修改时间
                             int64_t *createTime); //创建时间

            /// Read at maxium kBufferSize into buf_
            // return errno
            int readToBuffer(int *size);

            const char *buffer() const { return buf_; }

            static const int kBufferSize = 64 * 1024;

        private:
            int fd_;
            int err_;
            char buf_[kBufferSize];
        };

        // read the file content, returns errno if error happens. 是对 ReadSmallFile 的包装
        template <typename String>
        int readFile(StringArg filename,
                     int maxSize,
                     String *content,
                     int64_t *fileSize = NULL,
                     int64_t *modifyTime = NULL,
                     int64_t *createTime = NULL)
        {
            ReadSmallFile file(filename); // RAII，通过析构函数释放资源
            return file.readToString(maxSize, content, fileSize, modifyTime, createTime);
        }

        // not thread safe  用来写日志，使用的fopen、fwrite_unlocked、fflush、fclose
        class AppendFile : noncopyable
        {
        public:
            explicit AppendFile(StringArg filename);

            ~AppendFile();

            void append(const char *logline, size_t len);

            void flush();

            off_t writtenBytes() const { return writtenBytes_; }

        private:
            size_t write(const char *logline, size_t len);

            FILE *fp_;
            char buffer_[64 * 1024]; // 文件流缓冲区，供系统调用使用
            off_t writtenBytes_;
        };

    } // namespace FileUtil
} // namespace mymuduo

#endif
