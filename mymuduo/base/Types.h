#ifndef MY_MUDUO_BASE_TYPES_H
#define MY_MUDUO_BASE_TYPES_H

#include <stdint.h>
#include <string.h> // memset
#include <string>

#ifndef NDEBUG
#include <cassert>
#endif

///
/// The most common stuffs.
///
namespace mymuduo
{

    using std::string;
    // 简化了用 memset 初始化的使用
    inline void memZero(void *p, size_t n)
    {
        memset(p, 0, n);
    }

    /** 关于类型转换
     * 隐式类型转换：隐式类型转化是编译器默默地、隐式地、偷偷地进行的类型转换，这种转换不需要程序员干预，会自动发生
     * 强制类型转换: C++中有四种强制转换 const_cast，static_cast，dynamic_cast，reinterpret_cast
     *
     * 在类层次结构中基类（父类）和派生类（子类）之间指针或引用的转换时，
     * 进行上行转换（把派生类的指针或引用转换成基类表示）是安全的；
     * 进行下行转换（把基类指针或引用转换成派生类表示）时，由于没有动态类型检查，所以是不安全的。
     */

    // Taken from google-protobuf stubs/common.h
    //
    // Protocol Buffers - Google's data interchange format
    // Copyright 2008 Google Inc.  All rights reserved.
    // http://code.google.com/p/protobuf/
    //
    // Redistribution and use in source and binary forms, with or without
    // modification, are permitted provided that the following conditions are
    // met:
    //
    //     * Redistributions of source code must retain the above copyright
    // notice, this list of conditions and the following disclaimer.
    //     * Redistributions in binary form must reproduce the above
    // copyright notice, this list of conditions and the following disclaimer
    // in the documentation and/or other materials provided with the
    // distribution.
    //     * Neither the name of Google Inc. nor the names of its
    // contributors may be used to endorse or promote products derived from
    // this software without specific prior written permission.
    //
    // THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    // "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    // LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    // A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    // OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    // SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    // LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    // DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    // THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    // (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    // OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    // Author: kenton@google.com (Kenton Varda) and others
    //
    // Contains basic types and utilities used by the rest of the library.

    //
    // Use implicit_cast as a safe version of static_cast or const_cast
    // for upcasting in the type hierarchy (i.e. casting a pointer to Foo
    // to a pointer to SuperclassOfFoo or casting a pointer to Foo to
    // a const pointer to Foo).
    // When you use implicit_cast, the compiler checks that the cast is safe.
    // Such explicit implicit_casts are necessary in surprisingly many
    // situations where C++ demands an exact type match instead of an
    // argument type convertable to a target type.
    //
    // The From type can be inferred, so the preferred syntax for using
    // implicit_cast is the same as for static_cast etc.:
    //
    //   implicit_cast<ToType>(expr)
    //
    // implicit_cast would have been part of the C++ standard library,
    // but the proposal was submitted too late.  It will probably make
    // its way into the language in the future.

    /** implicit_cast 简单的隐式转换
     * 用于在继承关系中， 子类指针转化为父类指针；
     * 隐式转换inferred 推断，因为 from type 可以被推断出来，所以使用方法和 static_cast 相同
     * 在up_cast时应该使用 implicit_cast 替换 static_cast,因为前者比后者要安全。
     *
     * 在菱形继承中，static_cast把最底层的对象可以为中层对象，这样编译可以通过，但是在运行时可能崩溃
     * implicit_cast 就不会有这个问题，在编译时就会给出错误信息
     */
    template <typename To, typename From>
    inline To implicit_cast(From const &f)
    {
        return f;
    }

    // When you upcast (that is, cast a pointer from type Foo to type
    // SuperclassOfFoo), it's fine to use implicit_cast<>, since upcasts
    // always succeed.  When you downcast (that is, cast a pointer from
    // type Foo to type SubclassOfFoo), static_cast<> isn't safe, because
    // how do you know the pointer is really of type SubclassOfFoo?  It
    // could be a bare Foo, or of type DifferentSubclassOfFoo.  Thus,
    // when you downcast, you should use this macro.  In debug mode, we
    // use dynamic_cast<> to double-check the downcast is legal (we die
    // if it's not).  In normal mode, we do the efficient static_cast<>
    // instead.  Thus, it's important to test in debug mode to make sure
    // the cast is legal!
    //    This is the only place in the code we should use dynamic_cast<>.
    // In particular, you SHOULDN'T be using dynamic_cast<> in order to
    // do RTTI (eg code like this:
    //    if (dynamic_cast<Subclass1>(foo)) HandleASubclass1Object(foo);
    //    if (dynamic_cast<Subclass2>(foo)) HandleASubclass2Object(foo);
    // You should design the code some other way not to need this.

    /** down_cast 向下转换
     * down_cast在debug模式下内部使用了dynamic_cast进行验证，在release下使用static_cast替换dynamic_cast。
     * 为什么使用down_cast而不直接使用dynamic_cast?
     * 1. 因为但凡程序设计正确，dynamic_cast就可以用static_cast来替换，而后者比前者更有效率。
     * 2. dynamic_cast可能失败(在运行时crash)，运行时RTTI不是好的设计，不应该在运行时RTTI，或者需要RTTI时一般都有更好的选择。
     */
    template <typename To, typename From> // use like this: down_cast<T*>(foo);
    inline To down_cast(From *f)          // so we only accept pointers
    {
        // Ensures that To is a sub-type of From *.  This test is here only
        // for compile-time type checking, and has no overhead in an
        // optimized build at run-time, as it will be optimized away
        // completely.
        if (false)
        {
            implicit_cast<From *, To>(0);
        }

#if !defined(NDEBUG) && !defined(GOOGLE_PROTOBUF_NO_RTTI)
        assert(f == NULL || dynamic_cast<To>(f) != NULL); // RTTI: debug mode only!
#endif
        return static_cast<To>(f);
    }

} // namespace mymuduo

#endif // MY_MUDUO_BASE_TYPES_H
