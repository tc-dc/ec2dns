cmake_minimum_required(VERSION 3.3)
project(ec2dns)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(Boost_USE_STATIC_LIBS   ON)
set(Boost_USE_MULTITHREADED ON)

set(AWS_SDK_INCLUDE_PATH /usr/local/include)
set(AWS_SDK_PATHS /usr/local/lib/mac /usr/local/lib/linux/intel64 /usr/local/lib/linux/intel64/Release)
find_package(Boost 1.53 COMPONENTS regex REQUIRED)

find_library(AWS_SDK_LIB_CORE aws-cpp-sdk-core PATHS ${AWS_SDK_PATHS})
find_library(AWS_SDK_LIB_EC2 aws-cpp-sdk-ec2 PATHS ${AWS_SDK_PATHS})

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${AWS_SDK_INCLUDE_PATH})
include_directories("./include")

SET(SRCS include/Ec2DnsClient.h src/Ec2DnsClient.cpp src/dlz_aws.cpp)
SET(LIBS ${AWS_SDK_LIB_EC2} ${AWS_SDK_LIB_CORE} curl ssl crypto ${Boost_REGEX_LIBRARY_RELEASE})

add_library(ec2dns SHARED ${SRCS})
add_executable(ec2dnsexe ${SRCS} src/test.cpp)

target_link_libraries(ec2dns ${LIBS})
target_link_libraries(ec2dnsexe ${LIBS})
