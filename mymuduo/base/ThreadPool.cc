#include "mymuduo/base/ThreadPool.h"
#include "mymuduo/base/Exception.h"

#include <assert.h>
#include <stdio.h>

using namespace mymuduo;

ThreadPool::ThreadPool(const string &nameArg)
    : mutex_(),
      notEmpty_(mutex_),
      notFull_(mutex_),
      name_(nameArg),
      maxQueueSize_(0),
      running_(false) {}

ThreadPool::~ThreadPool()
{
    if (running_)
    {
        stop();
    }
}

void ThreadPool::start(int numThreads)
{
    assert(threads_.empty());
    // 线程开始时不需要加锁，是因为此时只是创建空线程，并没有具体的任务来进行调度，故为单线程
    running_ = true;
    threads_.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i)
    {
        char id[32];
        snprintf(id, sizeof id, "%d", i + 1);
        threads_.emplace_back(new mymuduo::Thread(
            std::bind(&ThreadPool::runInThread, this), name_ + id));
        threads_[i]->start(); // 开启线程
    }
    if (numThreads == 0 && threadInitCallback_)
    {
        threadInitCallback_();
    }
}

void ThreadPool::stop()
{
    {
        // stop 的时候涉及到共享数据的修改（running_），故需要加锁
        MutexLockGuard lock(mutex_);
        // 设置 running_ 来停止各线程的 loop
        running_ = false;
        // 唤醒仍在等待中的线程
        notEmpty_.notifyAll();
        notFull_.notifyAll();
    }
    for (auto &thr : threads_)
    {
        //
        thr->join();
    }
}

size_t ThreadPool::queueSize() const
{
    MutexLockGuard lock(mutex_);
    return queue_.size();
}

void ThreadPool::run(Task task)
{
    // 当线程池大小为 0 时，由ThreadPool 所在的线程负责执行任务
    if (threads_.empty())
    {
        task();
    }
    else
    {
        // 消费者/生产者 队列，参考 BlockingQueue 的实现
        // 因为要自己控制队列最大长度，所以没有使用 BlockingQueue
        MutexLockGuard lock(mutex_);
        while (isFull() && running_)
        {
            notFull_.wait();
        }
        // 当线程被唤醒的原因是调用了 stop，就会直接返回到 loop中，并退出loop
        // 所以 stop的实际作用是停止所有的 run动作，不再添加新的任务到队列
        if (!running_)
            return;
        assert(!isFull());

        queue_.push_back(std::move(task));
        notEmpty_.notify();
    }
}

ThreadPool::Task ThreadPool::take()
{
    MutexLockGuard lock(mutex_);
    // always use a while-loop, due to spurious wakeup
    // 使用 while 来防止虚假唤醒
    while (queue_.empty() && running_)
    {
        notEmpty_.wait();
    }
    Task task;
    // 可见，即使调用了 stop，队列中的任务也会被线程所执行（在最后一次 loop中）
    if (!queue_.empty())
    {
        task = queue_.front();
        queue_.pop_front();
        if (maxQueueSize_ > 0)
        {
            notFull_.notify();
        }
    }
    return task;
}

bool ThreadPool::isFull() const
{
    // 该函数必须在 mutex_ 锁住的情况下执行，为了防止并发访问
    mutex_.assertLocked();
    // maxQueueSize_==0 时表示无界队列
    return maxQueueSize_ > 0 && queue_.size() >= maxQueueSize_;
}

void ThreadPool::runInThread()
{
    try
    {
        if (threadInitCallback_)
        {
            threadInitCallback_();
        }
        while (running_)
        {
            Task task(take());
            if (task)
            {
                task();
            }
        }
    }
    catch (const Exception &ex)
    {
        fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
        fprintf(stderr, "reason: %s\n", ex.what());
        fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
        abort();
    }
    catch (const std::exception &ex)
    {
        fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
        fprintf(stderr, "reason: %s\n", ex.what());
        abort();
    }
    catch (...)
    {
        fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
        throw; // rethrow
    }
}
