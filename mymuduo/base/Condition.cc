#include "mymuduo/base/Condition.h"
#include <errno.h>

using namespace mymuduo;

bool Condition::waitForSeconds(double seconds)
{
    // time wait 需要一个表示绝对时间的 struct timespec 作为超时时间参数
    struct timespec abstime;
    /**
     * 函数 "clock_gettime" 是基于Linux C语言的时间函数,可以用于计算时间，有秒和纳秒两种精度。
     * 函数原型：
     *      int clock_gettime(clockid_t clk_id, struct timespec *tp);
     * 其中，cld_id类型四种：   
     *      a、CLOCK_REALTIME:系统实时时间,随系统实时时间改变而改变
     *      b、CLOCK_MONOTONIC,从系统启动这一刻起开始计时,不受系统时间被用户改变的影响
     *      c、CLOCK_PROCESS_CPUTIME_ID,本进程到当前代码系统CPU花费的时间
     *      d、CLOCK_THREAD_CPUTIME_ID,本线程到当前代码系统CPU花费的时间
     * struct timespec {
     *      time_t tv_sec; // 秒
     *      long tv_nsec; // 纳秒
     * };
     */
    clock_gettime(CLOCK_REALTIME, &abstime);
    const int64_t kNanoSecondsPerSecond = 1000000000;
    int64_t nanoseconds = static_cast<int64_t>(seconds * kNanoSecondsPerSecond);

    abstime.tv_sec += static_cast<time_t>((abstime.tv_nsec + nanoseconds) / kNanoSecondsPerSecond);
    abstime.tv_nsec = static_cast<long>((abstime.tv_nsec + nanoseconds) % kNanoSecondsPerSecond);

    MutexLock::UnassignGuard ug(mutex_);
    return ETIMEDOUT == pthread_cond_timedwait(&pcond_, mutex_.getPthreadMutex(), &abstime);
}