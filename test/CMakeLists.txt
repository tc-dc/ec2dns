project(ec2dns-tests)

include_directories("external/include")
include_directories("include")

set(TEST_SRCS
        external/src/gtest/gtest-all.cc
        external/src/gmock/gmock-all.cc
        src/RunTests.cpp
        src/HostMatcherTests.cpp
        src/ReverseLookupHelperTests.cpp
        src/Ec2DnsTests.cpp)

add_executable(ec2dns-tests ${TEST_SRCS})
target_link_libraries(ec2dns-tests ec2dns)

