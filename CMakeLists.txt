cmake_minimum_required(VERSION 3.10)
project(GT_SD06_Module_RTPSender)
set(CMAKE_CXX_STANDARD 11)

include_directories(include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdlib/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdlib/include/live555/BasicUsageEnvironment)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdlib/include/live555/UsageEnvironment)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdlib/include/live555/liveMedia)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdlib/include/live555/groupsock)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdlib/lib/live555)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdlib/lib/openssl)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdlib/lib/curl)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdlib/include/recordlib)

link_libraries(
    -lpthread
    -lrt
    -lBasicUsageEnvironment
    -lUsageEnvironment
    -lliveMedia
    -lgroupsock
    -lssl
    -lcrypto
    -lcurl
    -lrecordlib
)

add_subdirectory(src)
add_executable(module_rtpsender ${ALL_SRCS})


target_link_libraries(module_rtpsender
    pthread
    rt
    BasicUsageEnvironment
    UsageEnvironment
    liveMedia
    groupsock
    ssl
    crypto
    curl
    recordlib
)



