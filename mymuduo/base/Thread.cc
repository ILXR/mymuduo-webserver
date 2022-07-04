#include "mymuduo/base/Thread.h"
#include "mymuduo/base/Timestamp.h"
#include "mymuduo/base/Exception.h"
#include "mymuduo/base/CurrentThread.h"

// #include <muduo/base/Logging.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <type_traits>
#include <sys/types.h>
#include <sys/syscall.h>
#include <linux/unistd.h>

namespace mymuduo
{
    namespace detail
    {

        pid_t gettid()
        {
            return static_cast<pid_t>(::syscall(SYS_gettid));
        }

        void afterFork()
        {
            CurrentThread::t_cachedTid = 0;
            CurrentThread::t_threadName = "main";
            CurrentThread::tid();
        }

        /**
         * ThreadNameInitializer进行主线程初始化操作（利用全局变量）：
         * 包括设置默认的线程name、缓存线程id。
         * 如果进行了fork，那么在子进程中运行afterFork函数进行同样的初始化工作。
         */
        class ThreadNameInitializer
        {
        public:
            ThreadNameInitializer()
            {
                CurrentThread::t_threadName = "main";
                CurrentThread::tid();
                pthread_atfork(NULL, NULL, &afterFork);
                /**
                 * pthread_atfork(void (*prepare)(void），void (*parent)(void）, void(*child)(void))
                 * prepare在父进程fork创建子进程之前调用，这里可以获取父进程定义的所有锁；
                 * child fork返回之前在子进程环境中调用，在这里unlock prepare获得的锁；
                 * parent fork创建了子进程以后，但在fork返回之前在父进程的进程环境中调用的，在这里对prepare获得的锁进行解锁
                 */
            }
        };

        ThreadNameInitializer init;

        /**
         * 存放线程的全部信息
         * 是真正的调度实体
         */
        struct ThreadData
        {
            typedef mymuduo::Thread::ThreadFunc ThreadFunc;
            ThreadFunc func_;
            string name_;
            pid_t *tid_;
            CountDownLatch *latch_;

            ThreadData(ThreadFunc func,
                       const string &name,
                       pid_t *tid,
                       CountDownLatch *latch)
                : func_(std::move(func)),
                  name_(name),
                  tid_(tid),
                  latch_(latch)
            {
            }

            void runInThread()
            {
                *tid_ = CurrentThread::tid();
                tid_ = NULL;
                // Thread 对象阻塞直到此处调用 countDown，表示子线程开始运行了
                latch_->countDown();
                latch_ = NULL;

                CurrentThread::t_threadName = name_.empty() ? "muduoThread" : name_.c_str();
                ::prctl(PR_SET_NAME, CurrentThread::t_threadName); // prctl设置线程的名字
                try
                {
                    func_();
                    CurrentThread::t_threadName = "finished";
                }
                catch (const mymuduo::Exception &ex)
                {
                    CurrentThread::t_threadName = "crashed";
                    fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
                    fprintf(stderr, "reason: %s\n", ex.what());
                    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
                    abort();
                }
                catch (const std::exception &ex)
                {
                    CurrentThread::t_threadName = "crashed";
                    fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
                    fprintf(stderr, "reason: %s\n", ex.what());
                    abort();
                }
                catch (...)
                {
                    CurrentThread::t_threadName = "crashed";
                    fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
                    throw; // rethrow
                }
            }
        };

        /**
         * pthread_create只接受静态函数，所以我们需要一个跳板，这就是startThread存在的意义
         * 要在一个静态函数中使用类的动态成员（包括成员函数和成员变量），则只能通过如下两种方式来实现：
         *  ❑通过类的静态对象来调用。比如单体模式中，静态函数可以通过类的全局唯一实例来访问动态成员函数。
         *  ❑将类的对象作为参数传递给该静态函数，然后在静态函数中引用这个对象，并调用其动态方法。
         */
        void *startThread(void *obj)
        {
            ThreadData *data = static_cast<ThreadData *>(obj);
            data->runInThread();
            delete data;
            return NULL;
        }

    } // namespace detail

    /**
     *  @brief  调用一次muduo::CurrentThread::tid()把当前线程id缓存起来，以后再取线程id就不会陷入内核了
     *  只有在本线程第一次调用的时候才进行系统调用，以后都是直接从thread local缓存的线程id拿到结果
     */
    void CurrentThread::cacheTid()
    {
        if (t_cachedTid == 0)
        {
            t_cachedTid = detail::gettid();
            t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
        }
    }

    bool CurrentThread::isMainThread()
    {
        return tid() == ::getpid();
    }

    void CurrentThread::sleepUsec(int64_t usec)
    {
        struct timespec ts = {0, 0};
        ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
        ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
        ::nanosleep(&ts, NULL);
    }

    AtomicInt32 Thread::numCreated_;

    Thread::Thread(ThreadFunc func, const string &n)
        : started_(false),
          joined_(false),
          pthreadId_(0),
          tid_(0),
          func_(std::move(func)),
          name_(n),
          latch_(1)
    {
        setDefaultName();
    }

    Thread::~Thread()
    {
        if (started_ && !joined_)
        {
            // 析构的时候没有销毁持有的Pthreads，也就是说Thread的析构不会等待线程结束
            // 如果thread对象的生命期短于线程，那么析构时会自动detach线程，避免资源泄露
            pthread_detach(pthreadId_);
        }
    }

    void Thread::setDefaultName()
    {
        int num = numCreated_.incrementAndGet();
        if (name_.empty())
        {
            char buf[32];
            /**
             * 线程的默认name与它是第几个线程相关，std::string 没有format，那么格式化可以使用snprintf，
             * 或者使用ostringstream，或者boost::format也是可以的
             */
            snprintf(buf, sizeof buf, "Thread%d", num);
            name_ = buf;
        }
    }

    /**
     * 线程启动函数，调用pthread_create创建线程，线程函数为detail::startThread，
     * 传递给线程函数的参数data是在heap上分配的，data存放了线程真正要执行的函数记为func、线程id、线程name等信息。
     * detail::startThread会调用func启动线程，所以detail::startThread可以看成是一个跳板或中介。
     */
    void Thread::start()
    {
        assert(!started_);
        started_ = true;
        // FIXME: move(func_)
        detail::ThreadData *data = new detail::ThreadData(func_, name_, &tid_, &latch_);
        if (pthread_create(&pthreadId_, NULL, &detail::startThread, data))
        {
            started_ = false;
            delete data; // or no delete?
            // LOG_SYSFATAL << "Failed in pthread_create";
            perror("Failed in pthread_create\n");
        }
        else
        {
            // 如果线程创建成功，主线程阻塞在这里,等待子线程执行threadFunc之前被唤醒
            // 说明子线程执行成功，start 函数调用可以返回了
            latch_.wait();
            assert(tid_ > 0);
        }
    }

    int Thread::join()
    {
        assert(started_);
        assert(!joined_);
        joined_ = true;
        return pthread_join(pthreadId_, NULL);
    }

} // namespace mymuduo
