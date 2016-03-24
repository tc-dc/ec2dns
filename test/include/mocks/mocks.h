#include "gmock/gmock.h"
#include "aws/autoscaling/AutoScalingClient.h"
#include "aws/autoscaling/model/DescribeAutoScalingGroupsRequest.h"
#include "aws/ec2/EC2Client.h"
#include "aws/ec2/model/DescribeInstancesRequest.h"

#include "Ec2DnsClient.h"

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
        log_t *logCb,
        const std::shared_ptr<EC2Client> ec2Client,
        const std::shared_ptr<AutoScalingClient> asgClient,
        const Ec2DnsConfig config,
        std::shared_ptr<StatsReceiver> statsReceiver
    ) : Ec2DnsClient(logCb, ec2Client, asgClient, config, statsReceiver) {}

    void RefreshAutoScalerData(const Aws::Vector<Aws::EC2::Model::Instance>& instances) {
      this->_RefreshAutoscalerDataImpl(instances);
    }
};