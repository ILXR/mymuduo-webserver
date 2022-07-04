// A string like object that points into another piece of memory.
// Useful for providing an interface that allows clients to easily
// pass in either a "const char*" or a "string".
//
// Arghh!  I wish C++ literals were automatically of type "string".

#ifndef MY_MUDUO_BASE_STRINGPIECE_H
#define MY_MUDUO_BASE_STRINGPIECE_H

#include <string.h>
#include <iosfwd> // for ostream forward-declaration
#include <type_traits>

#include "mymuduo/base/Types.h"

namespace mymuduo
{

    // For passing C-style string argument to a function.
    class StringArg // copyable
    {
    public:
        StringArg(const char *str)
            : str_(str) {}

        StringArg(const string &str)
            : str_(str.c_str()) {}

        const char *c_str() const { return str_; }

    private:
        const char *str_;
    };

    /**
     * 当某个接口参数是接受字符串类型时，可以减少不必要的开销，该类型可以接受const char *，std::string，减少冗余代码编写
     *
     * C++里面有string和char*，如果用const string &s 做函数形参，可以同时兼容两种字符串。
     * 但当传入一个很长的 const char * 时，会分配内存并拷贝该字符串，生成一个较大的string对象。
     * 如果传入的是 const string ，同样会重新生成一个 string；
     *
     * 如果目的仅仅是读取字符串的值，用 StringPiece 仅仅是4+一个指针的内存开销，而且也保证了兼容性。
     * 所以这个类的目的是传入字符串的字面值，它内部的ptr_ 这块内存不归自己所有。所以不能做任何改动。
     * 另外这里的ptr_也声明为const，本意就是只读
     */
    class StringPiece
    {
    private:
        // 通过保存字符串指针和长度，来避免不必要的复制；
        // 开销很低，只需要sizeof(const char*) + sizeof(int)字节；
        // 内部的ptr_ 这块内存不归他所有，要确保在StringPiece生命期内，该数据可用
        const char *ptr_;
        int length_;

    public:
        // We provide non-explicit singleton constructors so users can pass
        // in a "const char*" or a "string" wherever a "StringPiece" is
        // expected.
        // 构造参数全部都是底层 const,只用于提取而不能修改指针指向的对象
        StringPiece()
            : ptr_(NULL), length_(0) {}
        StringPiece(const char *str)
            : ptr_(str), length_(static_cast<int>(strlen(ptr_))) {}
        StringPiece(const unsigned char *str)
            : ptr_(reinterpret_cast<const char *>(str)),
              length_(static_cast<int>(strlen(ptr_))) {}
        StringPiece(const string &str)
            : ptr_(str.data()), length_(static_cast<int>(str.size())) {}
        StringPiece(const char *offset, int len)
            : ptr_(offset), length_(len) {}

        // data() may return a pointer to a buffer with embedded NULs, and the
        // returned buffer may or may not be null terminated.  Therefore it is
        // typically a mistake to pass data() to a routine that expects a NUL
        // terminated string.  Use "as_string().c_str()" if you really need to do
        // this.  Or better yet, change your routine so it does not rely on NUL
        // termination.
        const char *data() const { return ptr_; }
        int size() const { return length_; }
        bool empty() const { return length_ == 0; }
        const char *begin() const { return ptr_; }
        const char *end() const { return ptr_ + length_; }

        void clear()
        {
            ptr_ = NULL;
            length_ = 0;
        }
        // ptr_ 是底层 const，所以顶层指针可以指向不同的对象
        void set(const char *buffer, int len)
        {
            ptr_ = buffer;
            length_ = len;
        }
        void set(const char *str)
        {
            ptr_ = str;
            length_ = static_cast<int>(strlen(str));
        }
        void set(const void *buffer, int len)
        {
            ptr_ = reinterpret_cast<const char *>(buffer);
            length_ = len;
        }

        char operator[](int i) const { return ptr_[i]; }

        void remove_prefix(int n)
        {
            ptr_ += n;
            length_ -= n;
        }

        void remove_suffix(int n) { length_ -= n; }

        bool operator==(const StringPiece &x) const
        {
            return ((length_ == x.length_) &&
                    (memcmp(ptr_, x.ptr_, length_) == 0));
        }
        bool operator!=(const StringPiece &x) const { return !(*this == x); }

// 二元判断式
#define STRINGPIECE_BINARY_PREDICATE(cmp, auxcmp)                                \
    bool operator cmp(const StringPiece &x) const                                \
    {                                                                            \
        int r = memcmp(ptr_, x.ptr_, length_ < x.length_ ? length_ : x.length_); \
        return ((r auxcmp 0) || ((r == 0) && (length_ cmp x.length_)));          \
    }
        STRINGPIECE_BINARY_PREDICATE(<, <);
        STRINGPIECE_BINARY_PREDICATE(<=, <);
        STRINGPIECE_BINARY_PREDICATE(>=, >);
        STRINGPIECE_BINARY_PREDICATE(>, >);
#undef STRINGPIECE_BINARY_PREDICATE

        int compare(const StringPiece &x) const
        {
            int r = memcmp(ptr_, x.ptr_, length_ < x.length_ ? length_ : x.length_);
            if (r == 0)
            {
                if (length_ < x.length_)
                    r = -1;
                else if (length_ > x.length_)
                    r = +1;
            }
            return r;
        }

        string as_string() const
        {
            return string(data(), size());
        }

        void CopyToString(string *target) const { target->assign(ptr_, length_); }

        // Does "this" start with "x"
        bool starts_with(const StringPiece &x) const
        {
            return ((length_ >= x.length_) && (memcmp(ptr_, x.ptr_, x.length_) == 0));
        }
    };

}

// ------------------------------------------------------------------
// Functions used to create STL containers that use StringPiece
//  Remember that a StringPiece's lifetime had better be less than
//  that of the underlying string or char*.  If it is not, then you
//  cannot safely store a StringPiece into an STL container
// ------------------------------------------------------------------

#ifdef HAVE_TYPE_TRAITS
// This makes vector<StringPiece> really fast for some STL implementations
// 由于StringPiece只持有目标指针，所以是POD类型，并且拥有平凡构造函数，
// 所以可以定义如下的type traits以指示STL采用更为高效的算法实现。
template <>
struct __type_traits<mymuduo::StringPiece>
{
    typedef __true_type has_trivial_default_constructor;
    typedef __true_type has_trivial_copy_constructor;
    typedef __true_type has_trivial_assignment_operator;
    typedef __true_type has_trivial_destructor;
    typedef __true_type is_POD_type;
};
#endif

// allow StringPiece to be logged
std::ostream &operator<<(std::ostream &o, const mymuduo::StringPiece &piece);

#endif // MUDUO_BASE_STRINGPIECE_H
