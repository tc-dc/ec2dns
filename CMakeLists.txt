cmake_minimum_required(VERSION 3.3)
project(ec2dns)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(Boost_USE_STATIC_LIBS   ON)
set(Boost_USE_MULTITHREADED ON)

set(AWS_SDK_INCLUDE_PATH /usr/local/include)
set(AWS_SDK_PATHS /usr/local/lib/mac /usr/local/lib/linux/intel64 /usr/local/lib/linux/intel64/Release)
find_package(Boost 1.53 COMPONENTS regex coroutine thread system context REQUIRED)

find_library(AWS_SDK_LIB_CORE aws-cpp-sdk-core PATHS ${AWS_SDK_PATHS})
find_library(AWS_SDK_LIB_EC2 aws-cpp-sdk-ec2 PATHS ${AWS_SDK_PATHS})
find_library(AWS_SDK_LIB_ASG aws-cpp-sdk-autoscaling PATHS, ${AWS_SDK_PATHS})

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${AWS_SDK_INCLUDE_PATH})
include_directories("include")
include_directories("external/include")

SET(SRCS
        src/KRandom.cpp
        src/Ec2DnsClient.cpp
        src/dlz_aws.cpp
        src/RequestThrottler.cpp
        src/ReverseLookupHelper.cpp
        src/Stats.cpp)

SET(LIBS ${AWS_SDK_LIB_EC2} ${AWS_SDK_LIB_CORE} ${AWS_SDK_LIB_ASG} curl ssl crypto ${Boost_LIBRARIES})

add_library(ec2dns SHARED ${SRCS})

target_link_libraries(ec2dns ${LIBS})

add_subdirectory(test)