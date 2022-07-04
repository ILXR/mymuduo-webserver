// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "mymuduo/base/CurrentThread.h"

#include <cxxabi.h>
#include <execinfo.h>
#include <stdlib.h>

namespace mymuduo
{
    namespace CurrentThread
    {
        __thread int t_cachedTid = 0;
        __thread char t_tidString[32];
        __thread int t_tidStringLength = 6;
        __thread const char *t_threadName = "unknown";
        /**
         * 静态断言优点：可以自定义断言失败之后的输出 便于debug找问题
         * is_same 通过模板特化来区分不同类型
         * template<typename, typename>     struct is_same              : public false_type { };
         * template<typename _Tp>           struct is_same<_Tp, _Tp>    : public true_type { };
         */
        static_assert(std::is_same<int, pid_t>::value, "pid_t should be int");

        /**
         *  @brief  输出当前函数调用的堆栈信息
         *  @param  demangle 表示是否要给 C++ 符号名称解码，将 C++ ABI 标识符转换成源程序标识符
         */
        string stackTrace(bool demangle)
        {
            string stack;
            const int max_frames = 200;
            void *frame[max_frames];
            /** int backtrace (void **buffer, int size);
             * 该函数用来获取当前线程的调用堆栈，获取的信息将会被存放在buffer中，它是一个指针数组。
             * 参数size用来指定buffer中可以保存多少个void* 元素。
             * 函数返回值是实际获取的指针个数，最大不超过size大小;
             * 在buffer中的指针实际是从堆栈中获取的返回地址,每一个堆栈框架有一个返回地址。
             * 注意某些编译器的优化选项对获取正确的调用堆栈有干扰，另外内联函数没有堆栈框架；
             * 删除框架指针也会使无法正确解析堆栈内容。
             */
            int nptrs = ::backtrace(frame, max_frames);
            /** char **backtrace_symbols (void *const *buffer, int size);
             * 该函数将从backtrace函数获取的信息转化为一个字符串数组。
             * 参数buffer是从backtrace函数获取的数组指针，size是该数组中的元素个数(backtrace的返回值)，
             * 函数返回值是一个指向字符串数组的指针,它的大小同buffer相同。
             * 每个字符串包含了一个相对于buffer中对应元素的可打印信息,
             * 它包括函数名，函数的偏移地址和实际的返回地址。
             * backtrace_symbols生成的字符串都是malloc出来的，但是不要最后一个一个的free，
             * 因为backtrace_symbols会根据backtrace给出的callstack层数，一次性的将malloc出来一块内存释放，
             * 所以，只需要在最后free返回指针就OK了。
             */
            char **strings = ::backtrace_symbols(frame, nptrs);
            /** void backtrace_symbols_fd (void *const *buffer, int size, int fd);
             * 除了上面两个函数，在 execinfo 文件中还提供了该接口
             * 该函数与backtrace_symbols函数具有相同的功能，不同的是它不会给调用者返回字符串数组，
             * 而是将结果写入文件描述符为fd的文件中，每个函数对应一行。它不需要调用malloc函数，
             * 因此适用于有可能调用该函数会失败的情况。
             */
            if (strings)
            {
                size_t len = 256;
                char *demangled = demangle ? static_cast<char *>(::malloc(len)) : nullptr;
                for (int i = 1; i < nptrs; ++i) // skipping the 0-th, which is this function
                {
                    if (demangle)
                    {
                        // https://panthema.net/2008/0901-stacktrace-demangled/
                        // bin/exception_test(_ZN3Bar4testEv+0x79) [0x401909]
                        char *left_par = nullptr;
                        char *plus = nullptr;
                        for (char *p = strings[i]; *p; ++p)
                        {
                            if (*p == '(')
                                left_par = p;
                            else if (*p == '+')
                                plus = p;
                        }

                        if (left_par && plus)
                        {
                            *plus = '\0';
                            int status = 0;
                            /**
                             * GNU libstdc++ 提供了 __cxa_demangle 接口来进行 demangle 工作
                             * 将 _ZN1N1AIiE1B4funcEi 格式的符号转化为 N ::A<int>::B ::func(int)
                             * 该方法只工作于 g++ 编译器下
                             */
                            char *ret = abi::__cxa_demangle(left_par + 1, demangled, &len, &status);
                            *plus = '+';
                            if (status == 0)
                            {
                                demangled = ret; // ret could be realloc()
                                stack.append(strings[i], left_par + 1);
                                stack.append(demangled);
                                stack.append(plus);
                                stack.push_back('\n');
                                continue;
                            }
                        }
                    }
                    // Fallback to mangled names
                    stack.append(strings[i]);
                    stack.push_back('\n');
                }
                free(demangled);
                free(strings);
            }
            return stack;
        }

    } // namespace CurrentThread
} // namespace muduo
