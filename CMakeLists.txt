cmake_minimum_required(VERSION 3.3)
project(awsdns)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories("/usr/local/include")
include_directories("./include")

link_directories("/usr/local/lib/mac")
link_directories("/usr/local/lib/linux/intel64/Release")
link_directories("/usr/local/lib/linux/intel64/")

add_library(awsdns SHARED include/Ec2DnsClient.h src/Ec2DnsClient.cpp src/dlz_aws.cpp)

target_link_libraries(awsdns aws-cpp-sdk-ec2)
target_link_libraries(awsdns aws-cpp-sdk-core)
target_link_libraries(awsdns curl)
target_link_libraries(awsdns ssl)
