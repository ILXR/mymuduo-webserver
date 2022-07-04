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
        /**
         * 这里主要运用了c++11的关键字default和delete。
         * delete：表示noncopyable类及其子类都不能进行拷贝构造和赋值的操作；
         * default：表示使用默认的构造函数和析构函数
         * 
         * 当我们自己定义了带有参数的构造函数后，默认构造函数不再被使用。
         * 如果用defalut关键字修饰默认构造函数，可以保证默认构造函数继续被使用
         */
        noncopyable &operator=(const noncopyable &) = delete;
        noncopyable(const noncopyable &) = delete;

    public:
        noncopyable() = default;
        ~noncopyable() = default;
    };
};

#endif // !MY_NOCOPYABLE_H