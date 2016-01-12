cmake_minimum_required(VERSION 3.3)
project(ec2dns)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories("/usr/local/include")
include_directories("./include")

link_directories("/usr/local/lib/mac")
link_directories("/usr/local/lib/linux/intel64/Release")
link_directories("/usr/local/lib/linux/intel64/")

SET(SRCS include/Ec2DnsClient.h src/Ec2DnsClient.cpp src/dlz_aws.cpp)
SET(LIBS aws-cpp-sdk-ec2 aws-cpp-sdk-core curl ssl)

add_library(ec2dns SHARED ${SRCS})
add_executable(ec2dnsexe ${SRCS} src/test.cpp)

target_link_libraries(ec2dns ${LIBS})
target_link_libraries(ec2dnsexe ${LIBS})