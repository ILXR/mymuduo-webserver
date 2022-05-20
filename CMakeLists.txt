cmake_minimum_required(VERSION 3.0.0)

project(mymuduo LANGUAGES C CXX VERSION 1.0)

option(MYMUDUO_BUILD_EXAMPLES "MYMUDUO_BUILD_EXAMPLES On" ON)

# 为当前及其下级目录打开测试功能。也可参见add_test命令。
# 注意，ctest需要在构建跟目录下找到一个测试文件。因此，这个命令应该在源文件目录的根目录下。
enable_testing()

# CMake中有一个变量CMAKE_BUILD_TYPE，可以取值枚举入下：Debug Release RelWithDebInfo 和 MinSizeRel
# 值为Debug的时候，Cmake会使用变量 CMAKE_CXX_FLAGS_DEBUG 和 CMAKE_C_FLAGS_DEBUG 中的字符串作为编译选项生成Makefile，
# 值为Release的时候，工程会使用变量 CMAKE_CXX_FLAGS_RELEASE 和 CMAKE_C_FLAGS_RELEASE 选项生成Makefile。
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CXX_FLAGS
    -g
    # D 选项是在使用gcc/g++编译的时候定义宏的
    # gcc -DDEBUG -D 后面直接跟宏名字，相当于定义这个宏，默认这个宏的内容是1
    # gcc -DNAME=Peter -D 后面跟 key=value 表示定义key这个宏，它的内容是value
    # -DVALGRIND
    -DCHECK_PTHREAD_RETURN_VALUE
    -D_FILE_OFFSET_BITS=64
    # -Wall 编译后显示所有警告
    -Wall
    # -w 关闭编译时的警告，也就是编译后不显示任何warning
    # -W 类似 -Wall，会显示警告，但是只显示编译器认为会出现错误的警告
    -Wextra # 它会打开-Wall选项没有打开的一些编译警告
    -Werror # 把所有的警告标识为错误
    -Wconversion
    -Wno-unused-parameter
    -Wold-style-cast
    -Woverloaded-virtual
    -Wpointer-arith
    -Wshadow
    -Wwrite-strings
    -march=native
    # -MMD # 生成文件关联的信息。包含目标文件所依赖的所有源代码，但是忽略 include ，并且将结果导入.d文件
    -std=c++11
    -rdynamic # 选项 -rdynamic 用来通知链接器将所有符号添加到动态符号表中
    # （目的是能够通过使用 dlopen 来实现向后跟踪）
    )
# 这里的 CXX_FLAGS 用set赋值后，使用 ";" 进行连接
message("CXX_FLAGS : ${CXX_FLAGS}")
# gcc 可以为指定架构生成汇编代码
# -m32 生成32位机器的汇编代码；
# -m64则生成64位机器汇编代码；
# 由于64位机器的寄存器比32位机器多很多，所以GCC编译器会尽量使用寄存器来传递参数，而不是32位机器下的压栈。
if(CMAKE_BUILD_BITS EQUAL 32)
  list(APPEND CXX_FLAGS "-m32")
endif()

# Clang 提供的特性
# -W<warning> 开启指定的Warning
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(APPEND CXX_FLAGS "-Wno-null-dereference")
  list(APPEND CXX_FLAGS "-Wno-sign-conversion")
  list(APPEND CXX_FLAGS "-Wno-unused-local-typedef")
  list(APPEND CXX_FLAGS "-Wthread-safety")
  list(REMOVE_ITEM CXX_FLAGS "-rdynamic")
endif()
# string(REPLACE <match_string>
# <replace_string> <output_variable>
# <input> [<input>...])
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

# debug版本的项目生成的可执行文件需要有调试信息并且不需要进行优化，
# 而release版本的不需要调试信息但是需要优化。
# 这些特性在gcc/g++中通过编译时的参数来决定的，
# 如果将优化程度调到最高需要设置 -O3 ，最低的是 -O0 即不做优化，
# 添加调试信息的参数是 -g  -ggdb，如果不添加这个参数，调试信息就不会被包含在生成的二进制中
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

find_package(Boost REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
message("Boost_INCLUDE_DIRS : ${Boost_INCLUDE_DIRS}")
include_directories(${PROJECT_SOURCE_DIR})
message("PROJECT_SOURCE_DIR : ${PROJECT_SOURCE_DIR}")

add_subdirectory(mymuduo/base)
add_subdirectory(mymuduo/net)