#ifndef MY_MUDUO_CURRENTTHREAD_H
#define MY_MUDUO_CURRENTTHREAD_H

#include "mymuduo/base/Types.h"

namespace mymuduo
{
    namespace CurrentThread
    {
        /** thread - internal - variable
         * __thread是GCC内置的线程局部存储设施，其存储效率可以和全局变量相比；
         * __thread变量在每一个线程中都有一份独立实例，各线程值是互不干扰的。
         * 只能修饰POD类型，可用于修饰全局变量，函数内的静态变量，
         * 不能修饰函数的局部变量或class的普通成员变量。且__thread变量值只能初始化为编译器常量
         * 
         * __thread限定符(specifier)可以单独使用，也可带有extern或static限定符，但不能带有其它存储类型的限定符。
         * __thread可用于全局的静态文件作用域，静态函数作用域或一个类中的静态数据成员。不能用于块作用域，自动或非静态数据成员。
         */
        extern __thread int t_cachedTid;
        extern __thread char t_tidString[32];
        extern __thread int t_tidStringLength;
        extern __thread const char *t_threadName;
        void cacheTid();

        inline int tid()
        {
            if (__builtin_expect(t_cachedTid == 0, 0))
            {
                cacheTid();
            }
            return t_cachedTid;
        }

        inline const char *tidString() // for logging
        {
            return t_tidString;
        }

        inline int tidStringLength() // for logging
        {
            return t_tidStringLength;
        }

        inline const char *name()
        {
            return t_threadName;
        }

        bool isMainThread();

        void sleepUsec(int64_t usec); // for testing

        string stackTrace(bool demangle);
    } // namespace CurrentThread
} // namespace mymuduo

#endif // MY_MUDUO_CURRENTTHREAD_H
