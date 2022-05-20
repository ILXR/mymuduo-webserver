#ifndef MY_NONCOPYABLE_H
#define MY_NONCOPYABLE_H

namespace mymuduo
{
    /**
     * 不可复制的类，即将拷贝构造和拷贝赋值都禁用的类
     * 直接继承该基类即可
     */
    class noncopyable
    {
    private:
        noncopyable &operator=(const noncopyable &) = delete;
        noncopyable(const noncopyable &) = delete;

    public:
        noncopyable() = default;
        ~noncopyable() = default;
    };
};

#endif // !MY_NOCOPYABLE_H