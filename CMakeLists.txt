cmake_minimum_required(VERSION 3.0.0)
project(mymuduo VERSION 0.1.0)

#设置c++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(CTest)
enable_testing()

#生成应用程序
ADD_EXECUTABLE(mymuduo test_Acceptor.cc EventLoop.cc Channel.cc Poller.cc Timer.hpp TimerQueue.cc EventLoopThread.cc Socket.cc SocketsOps.cc Acceptor.cc TcpServer.cc TcpConnection.cc Timestamp.cc)
target_link_libraries(mymuduo pthread muduo_base muduo_net)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
