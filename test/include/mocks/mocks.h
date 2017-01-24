#include "gmock/gmock.h"
#include "aws/autoscaling/AutoScalingClient.h"
#include "aws/autoscaling/model/DescribeAutoScalingGroupsRequest.h"
#include "aws/ec2/EC2Client.h"
#include "aws/ec2/model/DescribeInstancesRequest.h"

#include "CloudDnsClient.h"
#include "ec2/Ec2DnsClient.h"

using namespace Aws::EC2::Model;
using namespace Aws::AutoScaling::Model;

class MockEC2Client : public Aws::EC2::EC2Client {
public:
  MOCK_CONST_METHOD1(DescribeInstances, DescribeInstancesOutcome(const DescribeInstancesRequest& request));
};

class MockAutoScalingClient : public Aws::AutoScaling::AutoScalingClient {
public:
  MOCK_CONST_METHOD1(DescribeAutoScalingGroups, DescribeAutoScalingGroupsOutcome(const DescribeAutoScalingGroupsRequest &request));
};

class MockDnsClient : public Ec2DnsClient {
public:
    MockDnsClient(
        const std::shared_ptr<EC2Client> ec2Client,
        const std::shared_ptr<AutoScalingClient> asgClient,
        const CloudDnsConfig config,
        std::shared_ptr<StatsReceiver> statsReceiver
    ) : Ec2DnsClient(ec2Client, asgClient, config, statsReceiver) {}

    void RefreshAutoScalerData(const Aws::Vector<Aws::EC2::Model::Instance>& instances) {
      std::vector<::Instance> is;
      for (auto& i : instances) {
        is.push_back(::Instance(i.GetInstanceId(), i.GetPrivateIpAddress(), i.GetPlacement().GetAvailabilityZone()));
      }
      this->_RefreshAutoscalerDataImpl(is);
    }
};