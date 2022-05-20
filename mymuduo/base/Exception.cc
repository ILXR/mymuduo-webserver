#include "mymuduo/base/Exception.h"
#include "mymuduo/base/CurrentThread.h"

namespace mymuduo
{
    Exception::Exception(string what)
        : message_(std::move(what)),
          stack_(CurrentThread::stackTrace(false))
    {
    }
}