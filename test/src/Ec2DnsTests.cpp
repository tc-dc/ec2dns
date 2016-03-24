#include "gtest/gtest.h"

#include "Ec2DnsClient.h"
#include "mocks/mocks.h"

using namespace testing;
using namespace Aws::AutoScaling::Model;
using namespace Aws::EC2;
using namespace Aws::EC2::Model;

void _logcb(int, const char*, ...) {
  return;
}

DescribeInstancesOutcome _GetExpectedResponse() {
  return DescribeInstancesOutcome(
      DescribeInstancesResponse().AddReservations(
          Reservation().AddInstances(
              Aws::EC2::Model::Instance()
                  .WithPrivateIpAddress("10.1.2.3")
                  .WithPlacement(Placement().WithAvailabilityZone("us-east-1a"))
                  .WithInstanceId("i-1234567")
          ))
  );
}

DescribeAutoScalingGroupsOutcome _GetExpectedAsgResponse() {
  return DescribeAutoScalingGroupsOutcome(
      DescribeAutoScalingGroupsResult().AddAutoScalingGroups(
          AutoScalingGroup()
              .WithAutoScalingGroupName("testasg")
              .AddInstances(
                  Aws::AutoScaling::Model::Instance()
                    .WithInstanceId("i-0000001")
                    .WithHealthStatus("Healthy")
                    .WithLifecycleState(LifecycleState::InService)
              )
              .AddInstances(
                  Aws::AutoScaling::Model::Instance()
                      .WithInstanceId("i-0000002")
                      .WithHealthStatus("Healthy")
                      .WithLifecycleState(LifecycleState::Terminating)
              )
              .AddInstances(
                  Aws::AutoScaling::Model::Instance()
                      .WithInstanceId("i-0000003")
                      .WithHealthStatus("Unhealthy")
                      .WithLifecycleState(LifecycleState::InService)
              )
              .AddTags(
                  Aws::AutoScaling::Model::TagDescription()
                    .WithKey("twitter:aws:dns-alias")
                    .WithValue("testasg")
              )
      )
      .AddAutoScalingGroups(
          AutoScalingGroup()
            .WithAutoScalingGroupName("testasg2")
            .AddTags(
                Aws::AutoScaling::Model::TagDescription()
                  .WithKey("twitter:aws:dns-alias")
                  .WithValue("testasg2")
            )
      )
  );
}

Aws::Vector<Aws::EC2::Model::Instance> _GetAsgInstances() {
  return {
      Aws::EC2::Model::Instance()
          .WithInstanceId("i-0000001")
          .WithPrivateIpAddress("1.2.3.4"),
      Aws::EC2::Model::Instance()
          .WithInstanceId("i-0000002")
          .WithPrivateIpAddress("1.2.3.5"),
      Aws::EC2::Model::Instance()
          .WithInstanceId("i-0000003")
          .WithPrivateIpAddress("1.2.3.6")
  };
}

TEST(TestEc2DnsClient, TestEc2DnsClientResolveIp) {
  auto ptr = std::make_shared<MockEC2Client>();
  auto asgClient = std::make_shared<AutoScalingClient>();
  auto config = Ec2DnsConfig("tc", "10.0.0.0/23", "aws.test");
  EXPECT_CALL(*ptr, DescribeInstances(_))
      .WillOnce(Return(_GetExpectedResponse()));

  Ec2DnsClient dnsClient(&_logcb, ptr, asgClient, config, std::make_shared<StatsReceiver>());
  std::string ip;
  bool ret = dnsClient.TryResolveIp("i-1234567", "127.0.0.1", &ip);
  ASSERT_TRUE(ret);
  ASSERT_EQ(ip, "10.1.2.3");
}

TEST(TestEc2DnsClient, TestEc2DnsClientResolveHostname) {
  auto ptr = std::make_shared<MockEC2Client>();
  auto config = Ec2DnsConfig("tc", "10.0.0.0/23", "aws.test");
  EXPECT_CALL(*ptr, DescribeInstances(_))
      .WillOnce(Return(_GetExpectedResponse()));

  Ec2DnsClient dnsClient(&_logcb, ptr, std::make_shared<AutoScalingClient>(), config, std::make_shared<StatsReceiver>());
  std::string hostname;
  bool ret = dnsClient.TryResolveHostname("10.1.2.3", "127.0.0.1", &hostname);
  ASSERT_TRUE(ret);
  ASSERT_EQ(hostname, "ue1a-tc-1234567.aws.test.");
}

void _TestAsg(const std::string& dnsName, std::vector<std::string> expectedNodes, bool expectedSuccess) {
  auto ptr = std::make_shared<MockEC2Client>();
  auto asg = std::make_shared<MockAutoScalingClient>();
  auto config = Ec2DnsConfig("tc", "10.0.0.0/23", "aws.test");
  EXPECT_CALL(*asg, DescribeAutoScalingGroups(_))
      .WillOnce(Return(_GetExpectedAsgResponse()));

  MockDnsClient dnsClient(&_logcb, ptr, asg, config, std::make_shared<StatsReceiver>());
  dnsClient.RefreshAutoScalerData(_GetAsgInstances());

  std::vector<std::string> nodes;
  bool ret = dnsClient.TryResolveAutoscaler(dnsName, "127.0.0.1", &nodes);
  ASSERT_EQ(ret, expectedSuccess);
  ASSERT_EQ(nodes.size(), expectedNodes.size());
  ASSERT_EQ(nodes, expectedNodes);
}

TEST(TestEc2DnsClient, TestEc2DnsClientResolveAsg) {
  _TestAsg("testasg", {"1.2.3.4"}, true);
}

TEST(TestEc2DnsClient, TestEc2DnsClientResolveEmptyAsg) {
  _TestAsg("testasg2", {}, true);
}

TEST(TestEc2DnsClient, TestEc2DnsClientUnknownName) {
  _TestAsg("idontexist", {}, false);
}