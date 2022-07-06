// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MYMUDUO_BASE_PROCESSINFO_H
#define MYMUDUO_BASE_PROCESSINFO_H

// 定义实现了一些用于读取进程信息的函数
/** proc文件系统
 * 许多现代的unix系统实现提供了一个/proc虚拟文件系统。这个文件系统驻留于/proc目录，
 * 并包含了各种用于展示内核信息的文件，并允许进程通过常规I/O系统读取。
 * 需要注意的是/proc下保存的文件与子目录并非存储与磁盘，而是由内核在进程访问此类信息时动态创建的。
 */

#include "mymuduo/base/StringPiece.h"
#include "mymuduo/base/Timestamp.h"
#include "mymuduo/base/Types.h"
#include <vector>
#include <sys/types.h>

namespace mymuduo
{

  /** 进程相关信息
   * 对于系统中每个进程，内核提供了相应的目录，命名为/proc/PID，其中PID是进程ID。
   * 此目录下的文件与子目录包含了进程的相关信息。如查看/proc/1目录下文件可以查看init进程的信息。
   * /proc/PID/fd
   *    该目录为进程打开的每个文件描述符包含了一个符号连接，每个符号连接的名称与描述符的数值相匹配。每个进程可以通过/proc/self来访问自己的/proc/PID目录。
   * /proc/PID/task
   *    Linux2.4增加了线程组的概念。线程组中一些属性对于线程是唯一的，所以Linux2.4在/proc/PID目录下增加了task子目录。针对进程中每个线程，内核提供/proc/PID/task/TID命名的子目录（TID是线程id）。
   */
  namespace ProcessInfo
  {
    // 通过getpid获取
    pid_t pid();
    string pidString();
    // 通过getuid获取，是指执行程序的具体用户
    uid_t uid();
    // 通过getpwuid_r获取
    string username();
    // 通过geteuid获取，用户有效标识号，是指程序的所有者，程序在执行时拥有euid用户的权限
    uid_t euid();
    // 获取进程启动的时间
    Timestamp startTime();
    int clockTicksPerSecond();
    int pageSize();
    bool isDebugBuild(); // constexpr

    // 获取本地主机的标准主机名
    string hostname();
    string procname();
    StringPiece procname(const string &stat);

    /// read /proc/self/status
    string procStatus();

    /// read /proc/self/stat
    string procStat();

    /// read /proc/self/task/tid/stat
    string threadStat();

    /// readlink /proc/self/exe
    string exePath();

    int openedFiles();
    int maxOpenFiles();

    struct CpuTime
    {
      double userSeconds;
      double systemSeconds;

      CpuTime() : userSeconds(0.0), systemSeconds(0.0) {}

      double total() const { return userSeconds + systemSeconds; }
    };
    CpuTime cpuTime();

    int numThreads();
    std::vector<pid_t> threads();
  } // namespace ProcessInfo

} // namespace mymuduo

#endif // MUDUO_BASE_PROCESSINFO_H
