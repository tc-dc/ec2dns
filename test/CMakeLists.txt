project(ec2dns-tests)

include_directories("external/include")
set(TEST_SRCS external/src/gtest-all.cpp src/RunTests.cpp src/HostMatcherTests.cpp src/ReverseLookupHelperTests.cpp)

add_executable(ec2dns-tests ${TEST_SRCS})
target_link_libraries(ec2dns-tests ec2dns)

