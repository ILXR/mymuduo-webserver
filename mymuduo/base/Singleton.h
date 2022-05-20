#ifndef MY_SINGLETON_H
#define MY_SINGLETON_H

#include "mymuduo/base/noncopyable.h"
#include <pthread.h>

namespace mymuduo
{

    template <typename T>
    class Singleton : noncopyable
    {
    public:
        static T &instance()
        {
            pthread_once(&ponce_, &Singleton::init);
            return *value_;
        }

    private:
        Singleton();
        ~Singleton();
        static void init()
        {
            value_ = new T();
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