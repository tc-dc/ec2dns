#include "gtest/gtest.h"

#include "ReverseLookupHelper.h"

TEST(TestReverseLookupHelper, TestReverseLookupZones) {
  auto rl = ReverseLookupHelper(std::shared_ptr<Ec2DnsClient>());
  // Create a zone from 10.1.0.0 - 10.1.3.255
  rl.InitializeReverseLookupZones("10.1.0.0/22");

  ASSERT_TRUE(rl.IsReverseLookupZone("0.1.10.in-addr.arpa"));
  ASSERT_TRUE(rl.IsReverseLookupZone("1.1.10.in-addr.arpa"));
  ASSERT_TRUE(rl.IsReverseLookupZone("2.1.10.in-addr.arpa"));
  ASSERT_TRUE(rl.IsReverseLookupZone("3.1.10.in-addr.arpa"));
  ASSERT_FALSE(rl.IsReverseLookupZone("4.1.10.in-addr.arpa"));
}