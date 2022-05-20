add_executable(test_Acceptor test_Acceptor.cc)
target_link_libraries(test_Acceptor mymuduo_net)

add_executable(test_Channel test_Channel.cc)
target_link_libraries(test_Channel mymuduo_net)

add_executable(test_EventLoop test_EventLoop.cc)
target_link_libraries(test_EventLoop mymuduo_net)

add_executable(test_EventLoopThread test_EventLoopThread.cc)
target_link_libraries(test_EventLoopThread mymuduo_net)

add_executable(test_TcpClient test_TcpClient.cc)
target_link_libraries(test_TcpClient mymuduo_net)

add_executable(test_TcpServer test_TcpServer.cc)
target_link_libraries(test_TcpServer mymuduo_net)

add_executable(test_TimerQueue test_TimerQueue.cc)
target_link_libraries(test_TimerQueue mymuduo_net)
# add_test(NAME mytest
#       COMMAND testDriver --config $<CONFIG>
#                          --exe $<TARGET_FILE:myexe>)
# 向要由ctest运行的项目添加添加名为 <name> 的测试
# 测试名称可以包含任意字符，如果需要，可以用引号参数或括号参数表示
# 注意，只有在调用了enable_testing()命令时，CMake才会生成测试
# CTest模块会自动调用该命令，除非BUILD_TESTING选项被关闭
add_test(NAME test_TimerQueue COMMAND test_TimerQueue)