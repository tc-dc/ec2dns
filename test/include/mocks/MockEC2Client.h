#include "gmock/gmock.h"
#include "aws/ec2/EC2Client.h"
#include "aws/ec2/model/DescribeInstancesRequest.h"

using namespace Aws::EC2;

class MockEC2Client : public Aws::EC2::EC2Client {
public:
  MOCK_CONST_METHOD1(DescribeInstances, Model::DescribeInstancesOutcome(const Model::DescribeInstancesRequest& request));
};