include(CheckFunctionExists)

# check_function_exists(<function> <variable>)
# 检查系统库是否支持 function 并将结果写入 variable
check_function_exists(accept4 HAVE_ACCEPT4)
if(NOT HAVE_ACCEPT4)
    # directory source_file target 都可以设置 properties
    set_source_files_properties(SocketsOps.cc PROPERTIES COMPILE_FLAGS "-DNO_ACCEPT4")
endif()

set(net_SRCS
    Acceptor.cc
    Buffer.cc
    Channel.cc
    Connector.cc
    EventLoop.cc
    EventLoopThread.cc
    EventLoopThreadPool.cc
    InetAddress.cc
    Poller.cc
    poller/DefaultPoller.cc
    poller/EPollPoller.cc
    poller/PollPoller.cc
    Socket.cc
    SocketsOps.cc
    TcpClient.cc
    TcpConnection.cc
    TcpServer.cc
    Timer.hpp
    TimerQueue.cc
)

add_library(mymuduo_net ${net_SRCS})
target_link_libraries(mymuduo_net mymuduo_base)

install(TARGETS mymuduo_net DESTINATION lib)

# 这里没有包括 Connector.h Poller.h Socket.h 等是因为这些是不对用户暴露的 
set(HEADERS
    Buffer.h
    Callbacks.h
    Channel.h
    Endian.h
    EventLoop.h
    EventLoopThread.h
    EventLoopThreadPool.h
    InetAddress.h
    TcpClient.h
    TcpConnection.h
    TcpServer.h
    TimerId.h
)
install(FILES ${HEADERS} DESTINATION include/mymuduo/net)

if(MYMUDUO_BUILD_EXAMPLES)
  add_subdirectory(tests)
endif()