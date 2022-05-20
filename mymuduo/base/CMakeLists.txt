set(base_SRCS
    Condition.cc
    CountDownLatch.cc
    CurrentThread.cc
    Exception.cc
    Thread.cc
    Timestamp.cc
    )

add_library(mymuduo_base ${base_SRCS})
target_link_libraries(mymuduo_base 
    PUBLIC pthread 
    PUBLIC rt)

# 安装 mymuduo_base 静态库到 ${CMAKE_INSTALL_PREFIX}/lib
# TARGETS 安装支持 静态库、动态库、可执行文件
install(TARGETS mymuduo_base DESTINATION lib)

# file(GLOB variable [RELATIVE path] [globbingexpressions]...)
# GLOB 会产生一个由所有匹配globbing表达式的文件组成的列表，并将其保存到变量 variable 中
# 如果为一个表达式指定了RELATIVE标志，返回的结果将会是相对于给定路径的相对路径
file(GLOB HEADERS "*.h")
# FILES 安装普通文件
install(FILES ${HEADERS} DESTINATION include/mymuduo/base)