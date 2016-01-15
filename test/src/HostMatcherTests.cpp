#include "gtest.h"

#include "HostMatcher.h"
#include "Ec2DnsClient.h"

TEST(TestMatchesValidHostname, TestMatchesValidHostname) {
  auto config = Ec2DnsConfig();
  auto hm = HostMatcher(config);

  std::string instanceId, awsRegion;
  bool success = hm.TryMatch("ue1a-12345678-tc", &instanceId, &awsRegion);

  ASSERT_TRUE(success);
  ASSERT_EQ(instanceId, "i-12345678");
  ASSERT_EQ(awsRegion, "ue1");
}

TEST(TestFailsInvalidHostname, TestFailsInvalidHostname) {
  auto config = Ec2DnsConfig();
  auto hm = HostMatcher(config);

  std::string instanceId, awsRegion;
  bool success = hm.TryMatch("invalid-data", &instanceId, &awsRegion);

  ASSERT_FALSE(success);
}