add_executable(Acceptor_test Acceptor_test.cc)
target_link_libraries(Acceptor_test mymuduo_net)

add_executable(Channel_test Channel_test.cc)
target_link_libraries(Channel_test mymuduo_net)

add_executable(EventLoop_test EventLoop_test.cc)
target_link_libraries(EventLoop_test mymuduo_net)

add_executable(EventLoopThread_test EventLoopThread_test.cc)
target_link_libraries(EventLoopThread_test mymuduo_net)

add_executable(TcpClient_test TcpClient_test.cc)
target_link_libraries(TcpClient_test mymuduo_net)

add_executable(TcpServer_test TcpServer_test.cc)
target_link_libraries(TcpServer_test mymuduo_net)

add_executable(TimerQueue_test TimerQueue_test.cc)
target_link_libraries(TimerQueue_test mymuduo_net)
# add_test(NAME mytest
#       COMMAND testDriver --config $<CONFIG>
#                          --exe $<TARGET_FILE:myexe>)
# 向要由ctest运行的项目添加添加名为 <name> 的测试
# 测试名称可以包含任意字符，如果需要，可以用引号参数或括号参数表示
# 注意，只有在调用了enable_testing()命令时，CMake才会生成测试
# CTest模块会自动调用该命令，除非BUILD_TESTING选项被关闭
add_test(NAME TimerQueue_test COMMAND TimerQueue_test)