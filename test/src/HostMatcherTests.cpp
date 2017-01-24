#include "gtest/gtest.h"

#include "HostMatcher.h"
#include "CloudDnsClient.h"

TEST(TestHostMatcher, TestMatchesValidHostname) {
  auto config = CloudDnsConfig("tc", "10.0.0.0/23", "aws.test");
  auto hm = HostMatcher(config);

  std::string instanceId, awsRegion;
  bool success = hm.TryMatch("ue1a-tc-12345678", &instanceId, &awsRegion);

  ASSERT_TRUE(success);
  ASSERT_EQ(instanceId, "i-12345678");
  ASSERT_EQ(awsRegion, "ue1");
}

TEST(TestHostMatcher, TestFailsInvalidHostname) {
  auto config = CloudDnsConfig("tc", "10.0.0.0/23", "aws.test");
  auto hm = HostMatcher(config);

  std::string instanceId, awsRegion;
  bool success = hm.TryMatch("invalid-data", &instanceId, &awsRegion);

  ASSERT_FALSE(success);
}