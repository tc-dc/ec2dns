cmake_minimum_required(VERSION 3.3)
project(ec2dns)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(Boost_USE_STATIC_LIBS   ON)
set(Boost_USE_MULTITHREADED ON)

set(AWS_SDK_INCLUDE_PATH /usr/local/include)
set(JSONCPP_INCLUDE_DIRS /usr/include/jsoncpp)
set(AWS_SDK_PATHS /usr/local/lib)
find_package(Boost 1.53 COMPONENTS regex coroutine thread system context REQUIRED)
find_library(AWS_SDK_LIB_CORE aws-cpp-sdk-core PATHS ${AWS_SDK_PATHS})
find_library(AWS_SDK_LIB_EC2 aws-cpp-sdk-ec2 PATHS ${AWS_SDK_PATHS})
find_library(AWS_SDK_LIB_ASG aws-cpp-sdk-autoscaling PATHS ${AWS_SDK_PATHS})

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${AWS_SDK_INCLUDE_PATH})
include_directories(SYSTEM ${JSONCPP_INCLUDE_DIRS})
include_directories("include")
include_directories("external/include")
include_directories(SYSTEM "external/google/compute/")

SET(SRCS
        src/gce/GceDnsClient.cpp
        src/gce/InstanceCredentials.cpp
        src/ec2/Ec2DnsClient.cpp
        src/CloudDnsClient.cpp
        src/dlz_aws.cpp
        src/KRandom.cpp
        src/RequestThrottler.cpp
        src/ReverseLookupHelper.cpp
        src/Stats.cpp)

file(GLOB GOOGLE_COMPUTE_SDK external/google/compute/google/compute_api/*.cc)

SET(GOOGLE_LIBS
        googleapis_curl_http
        googleapis_http
        googleapis_internal
        googleapis_json
        googleapis_jsoncpp
        googleapis_oauth2
        googleapis_openssl_codec
        googleapis_utils)

SET(LIBS
        ${AWS_SDK_LIB_EC2}
        ${AWS_SDK_LIB_CORE}
        ${AWS_SDK_LIB_ASG}
        curl
        ssl
        crypto
        glog
        jsoncpp
        ${Boost_LIBRARIES}
        ${GOOGLE_LIBS})

add_library(ec2dns SHARED ${SRCS} ${GOOGLE_COMPUTE_SDK})

target_link_libraries(ec2dns ${LIBS})

add_subdirectory(test)

add_definitions(-DWITH_AWS)
add_definitions(-DWITH_GCE)