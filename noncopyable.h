#ifndef MY_NONCOPYABLE_H
#define MY_NONCOPYABLE_H

namespace mymuduo
{
    class noncopyable
    {
    private:
        noncopyable &operator=(const noncopyable &) = delete;
        nocopyable(const noncopyable &) = delete;

    public:
        nocopyable() = default;
        ~nocopyable() = default;
    };
};

#endif // !MY_NOCOPYABLE_H