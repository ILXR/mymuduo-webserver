set(http_SRCS
    HttpServer.cc
    HttpResponse.cc
    HttpContext.cc
)

add_library(mymuduo_http ${http_SRCS})
target_link_libraries(mymuduo_http mymuduo_net)

install(TARGETS mymuduo_http DESTINATION lib)
set(HEADERS
    HttpContext.h
    HttpRequest.h
    HttpResponse.h
    HttpServer.h
)
install(FILES ${HEADERS} DESTINATION include/mymuduo/http)

if(MYMUDUO_BUILD_EXAMPLES)
    add_executable(httpserver_test tests/HttpServer_test.cc)
    target_link_libraries(httpserver_test mymuduo_http)

    # if(BOOSTTEST_LIBRARY)
    # add_executable(httprequest_unittest tests/HttpRequest_unittest.cc)
    # target_link_libraries(httprequest_unittest muduo_http boost_unit_test_framework)
    # endif()
endif()