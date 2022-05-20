#ifndef MY_MUDUO_EXCEPTION_H
#define MY_MUDUO_EXCEPTION_H

#include "mymuduo/base/Types.h"
#include <exception>

namespace mymuduo
{

    class Exception : public std::exception
    {
    public:
        Exception(string what);
        ~Exception(){}

        const char *what() const noexcept override
        {
            return message_.c_str();
        }
        const char *stackTrace() const noexcept
        {
            return stack_.c_str();
        }

    private:
        string message_;
        string stack_;
    };
}

#endif