cmake_minimum_required(VERSION 3.3)
project(ec2dns)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories("/usr/local/include")
include_directories("./include")

link_directories("/usr/local/lib/mac")
link_directories("/usr/local/lib/linux/intel64/Release")
link_directories("/usr/local/lib/linux/intel64/")

add_library(ec2dns SHARED include/Ec2DnsClient.h src/Ec2DnsClient.cpp src/dlz_aws.cpp)

target_link_libraries(ec2dns aws-cpp-sdk-ec2)
target_link_libraries(ec2dns aws-cpp-sdk-core)
target_link_libraries(ec2dns curl)
target_link_libraries(ec2dns ssl)
