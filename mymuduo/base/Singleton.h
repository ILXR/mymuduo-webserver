#ifndef MY_SINGLETON_H
#define MY_SINGLETON_H

#include "mymuduo/base/noncopyable.h"
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>

namespace mymuduo
{
    namespace detail
    {
        /**
         * has_no_destroy 实现了在编译期间判断泛型T中是否存在no_destroy方法
         * 是对模板编程中的SFINAE（匹配失败不是异常）的应用
         */
        template <typename T>
        struct has_no_destory
        {
            template <typename C>
            static char test(decltype(&C::no_destory)); // 首先匹配 C 中 no_destory 类型
            template <typename C>
            static int32_t test(...); // 可变参数列表，可以匹配其他所有参数情况
            // test 定义了两种函数声明
            // 当参数为 decltype(&C::no_destory) 匹配成功时， value == True
            const static bool value = sizeof(test<T>(0)) == 1;
        };
    }

    template <typename T>
    class Singleton : noncopyable
    {
    public:
        static T &instance()
        {
            pthread_once(&ponce_, &Singleton::init);
            assert(value_ != NULL);
            return *value_;
        }

    private:
        Singleton();
        ~Singleton();
        static void init()
        {
            value_ = new T();
            if (detail::has_no_destory<T>::value)
            {
                ::atexit(destory);
            }
        }

        static void destory()
        {
            // 当 T 为 void 类型时，编译时会产生 warning
            typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
            T_must_be_complete_type dummy;
            (void)dummy;

            delete value_;
            value_ = NULL;
        }

        static pthread_once_t ponce_;
        static T *value_;
    };

    template <typename T>
    pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;
    template <typename T>
    T *Singleton<T>::value_ = NULL;
}

#endif // !MY_SINGLETON_H