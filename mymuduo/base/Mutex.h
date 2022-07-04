#ifndef MY_MUTEX_H
#define MY_MUTEX_H

#include "mymuduo/base/CurrentThread.h"
#include "mymuduo/base/noncopyable.h"
#include <assert.h>
#include <pthread.h>

// Thread safety annotations {
// https://clang.llvm.org/docs/ThreadSafetyAnalysis.html

/**
 * GNU C的一大特色（却不被初学者所知）就是__attribute__机制。
 * __attribute__关键字主要是用来在函数或数据声明中设置其属性。给函数赋给属性的主要目的在于让编译器进行优化
 * __attribute__可以设置函数属性(Function Attribute)、变量属性(Variable Attribute)和类型属性(Type Attribute)
 * __attribute__前后都有两个下划线，并且后面会紧跟一对原括弧，括弧里面是相应的__attribute__参数
 * __attribute__语法格式为：
 *      __attribute__ ( ( attribute-list ) )
 * 但是这里用到的线性安全注解是 clang 提供的，并且提供了官方推荐的宏定义
 * 必须定义了__SUPPORT_TS_ANNOTATION__或者__clang__，Thread Safety Annotations 才起作用，否则是no-op
 */

// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
// 这里 SWIG 是一个能将 C/C++ 程序与其他各种高级语言连接的开发工具
#if defined(__clang__) && (!defined(SWIG))
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x) // no-op
#endif

//该宏负责指定相关的类使用线程安全检测机制，即标准文档所说的功能，用于直接修饰类
#define CAPABILITY(x) \
    THREAD_ANNOTATION_ATTRIBUTE__(capability(x))
//负责实现RAII样式锁定的类属性，即在构造时获取能力，析构时释放能力。其他和CAPABILITY类似。
#define SCOPED_CAPABILITY \
    THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)
//声明数据成员受给定功能保护。对数据的读取操作需要共享访问，而写入操作需要独占访问。
#define GUARDED_BY(x) \
    THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))
//和GUARDED_BY类似，用于指针和智能指针，用户保护指针指向的数据。
#define PT_GUARDED_BY(x) \
    THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))
//需要在另一个能力获取之前被调用
#define ACQUIRED_BEFORE(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))
//需要在另一个能力获取之后被调用
#define ACQUIRED_AFTER(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))
//用来修饰函数，使其调用线程必须具有对给定功能的独占访问权限。
//被修饰的函数在进入前必须已经持有能力，函数退出时不在持有能力。
#define REQUIRES(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))
//和REQUIRES类似，只不过REQUIRES_SHARED可以共享地获取能力
#define REQUIRES_SHARED(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))
//用来修饰函数，使其调用线程必须具有对给定功能的独占访问权限。被修饰的函数在进入前必须持有能力。
#define ACQUIRE(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))
//和ACQUIRE相同，只是能力可以共享
#define ACQUIRE_SHARED(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))
//用来修饰函数，使其调用线程必须具有对给定功能的独占访问权限。被修饰的函数退出时不在持有能力。
#define RELEASE(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))
//和RELEASE相同，用于修饰释放可以共享的能力
#define RELEASE_SHARED(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))
//和RELEASE相同，用于修饰释放共享的能力和非共享的能力
#define TRY_ACQUIRE(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))
//尝试获取能力
#define TRY_ACQUIRE_SHARED(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))
//尝试获取共享的能力
#define EXCLUDES(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))
//修饰函数一定不能具有某项能力
#define ASSERT_CAPABILITY(x) \
    THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))
//修饰调用线程已经具有给定的能力。
#define ASSERT_SHARED_CAPABILITY(x) \
    THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))
//修饰函数负责返回给定的能力
#define RETURN_CAPABILITY(x) \
    THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))
//修饰函数，关闭能力检查
#define NO_THREAD_SAFETY_ANALYSIS \
    THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

// End of thread safety annotations }

#ifdef CHECK_PTHREAD_RETURN_VALUE

#ifdef NDEBUG
__BEGIN_DECLS
/**
 * 为了使 C 代码和 C++ 代码保持互相兼容的过程调用接口，需要在 C++ 代码里加上 extern “C” 作为符号声明的一部分
 * #if defined(__cplusplus)
 *     #define __BEGIN_DECLS   extern "C" {
 *     #define __END_DECLS     }
 * #else
 *     #define __BEGIN_DECLS
 *     #define __END_DECLS
 */
extern void __assert_perror_fail(int errnum,
                                 const char *file,
                                 unsigned int line,
                                 const char *function) noexcept __attribute__((__noreturn__));
__END_DECLS
#endif

/**
 * __builtin_expect(EXP, N) 意思是：EXP==N的概率很大
 * gcc 允许程序员将最有可能执行的分支告诉编译器，用于提高流水线效率，生成高效代码
 * 一般的使用方法是将__builtin_expect指令封装为LIKELY和UNLIKELY宏
 *   #define LIKELY(x) __builtin_expect(!!(x), 1) //x很可能为真
 *   #define UNLIKELY(x) __builtin_expect(!!(x), 0) //x很可能为假
 */
#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
                       if (__builtin_expect(errnum != 0, 0))    \
                         __assert_perror_fail (errnum, __FILE__, __LINE__, __func__); })

#else // CHECK_PTHREAD_RETURN_VALUE

/**
 * 对 pthread 系列函数进行检查
 * 函数执行成功会返回 0，否则返回错误编号
 */
#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
                       assert(errnum == 0); (void) errnum; })

#endif // CHECK_PTHREAD_RETURN_VALUE

namespace mymuduo
{

    /**
     * 封装了 pthread_mutex_t
     * 一般只能用来被 MutexLockGuard 持有并管理
     */
    class CAPABILITY("mutex") MutexLock : noncopyable
    {
    public:
        MutexLock() : holder_(0)
        {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
            MCHECK(pthread_mutex_init(&mutex_, &attr));
        }
        ~MutexLock()
        {
            // assert()在 release build里是空语句，所以发行版中不要使用 assert
            assert(holder_ == 0);
            MCHECK(pthread_mutex_destroy(&mutex_));
        }

        bool isLockedByThisThread() { return holder_ == CurrentThread::tid(); }
        void assertLocked() { assert(isLockedByThisThread()); }

        void lock() ACQUIRE()
        { // 仅供 MutexLockGuard 调用，调用前必须持有访问权限

            pthread_mutex_lock(&mutex_);
            assignHolder();
        }
        void unlock() RELEASE()
        { // 仅供 MutexLockGuard 调用，退出时不再持有mutex
            unassignHolder();
            pthread_mutex_unlock(&mutex_);
        }

        pthread_mutex_t *getPthreadMutex() { return &mutex_; }

    private:
        friend class Condition;

        /**
         * 使用栈上对象控制 MutexLock::holder_
         * 为了在 Condition 调用 wait 时保持 holder 的前后一致性
         * 构造时将 holder 置零
         * 析构时将 holder 赋值为当前线程id
         */
        class UnassignGuard : noncopyable // ???
        {
        public:
            explicit UnassignGuard(MutexLock &owner) : owner_(owner)
            {
                owner.unassignHolder();
            }
            ~UnassignGuard() { owner_.assignHolder(); }

        private:
            MutexLock &owner_;
        };

        void unassignHolder() { holder_ = 0; }
        void assignHolder() { holder_ = CurrentThread::tid(); }

        pthread_mutex_t mutex_;
        pid_t holder_;
    };

    /**
     * 使用栈上对象封装对 mutex 的操作
     * 构造时上锁，析构时释放锁
     */
    class MutexLockGuard : noncopyable
    {
    public:
        explicit MutexLockGuard(MutexLock &mutex) : mutex_(mutex) { mutex_.lock(); }
        ~MutexLockGuard() { mutex_.unlock(); }

    private:
        MutexLock &mutex_;
    };

}
/**
 * 为了防止出现生成 MutexLockGuard(mutex) 这种临时变量的情况
 * 临时变量创建后，如果没有被使用，就会马上销毁，结果没有锁住临界区
 *
 * 在命名空间和其他语言级概念“启动”之前，预处理器（在编译器编译之前）运行
 * 所以 define 在命名空间内外是一样的
 */
#define MutexLockGuard(x) static_assert(false, "missing mutex guard var name")

#endif // !MY_MUTEX_H