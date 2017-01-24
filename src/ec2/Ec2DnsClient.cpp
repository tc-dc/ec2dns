#include "ec2/Ec2DnsClient.h"

#include "aws/core/Aws.h"
#include "aws/core/auth/AWSCredentialsProvider.h"
#include "aws/core/utils/StringUtils.h"
#include "aws/core/utils/logging/AWSLogging.h"
#include "aws/core/utils/logging/DefaultLogSystem.h"

using namespace Aws::Utils;


bool Ec2DnsClient::_DescribeInstances(
    const std::string &instanceId,
    const std::string &ip,
    std::vector<Instance> *instances) {

  Aws::EC2::Model::DescribeInstancesRequest req;
  req.SetMaxResults(this->m_config.request_batch_size);
  if (instanceId.empty() && ip.empty()) {
    LOG(INFO) << "Getting all instances";
  }
  else if (ip.empty()) {
    req.AddInstanceIds(instanceId);
  }
  else {
    req.AddFilters(Aws::EC2::Model::Filter()
                       .WithName("private-ip-address")
                       .AddValues(ip));
  }

  std::vector<Aws::EC2::Model::DescribeInstancesResponse> responses;
  bool success = this->_CallApi<
      Aws::EC2::Model::DescribeInstancesRequest,
      Aws::EC2::Model::DescribeInstancesResponse,
      Aws::EC2::EC2Errors
  >("DescribeInstances", req, std::bind(&EC2Client::DescribeInstances, this->m_ec2Client, _1), &responses);

  if (!success) {
    return false;
  }

  for (const auto &resp : responses) {
    const auto reservs = resp.GetReservations();
    for (const auto &r : reservs) {
      const auto resInstances = r.GetInstances();
      for (const auto &i : resInstances) {
        instances->push_back(Instance(i.GetInstanceId(), i.GetPrivateIpAddress(), i.GetPlacement().GetAvailabilityZone()));
      }
    }
  }
  return true;
}


bool Ec2DnsClient::_DescribeAutoscalingGroups(
    std::unordered_map<std::string, const std::unordered_set<std::string>> *results
) {
  std::vector<Aws::AutoScaling::Model::DescribeAutoScalingGroupsResult> asgs;
  auto req = Aws::AutoScaling::Model::DescribeAutoScalingGroupsRequest();
  bool success = this->_CallApi<
      Aws::AutoScaling::Model::DescribeAutoScalingGroupsRequest,
      Aws::AutoScaling::Model::DescribeAutoScalingGroupsResult,
      Aws::AutoScaling::AutoScalingErrors
  >("DescribeAutoScalingGroups", req,
    std::bind(&AutoScalingClient::DescribeAutoScalingGroups, this->m_asgClient, _1), &asgs);

  if (!success)
    return false;

  for (const auto &resp : asgs) {
    for (const auto &asg : resp.GetAutoScalingGroups()) {
      for (const auto &tag : asg.GetTags()) {
        if (tag.GetKey() == this->m_config.asg_dns_tag) {
          const auto &dnsAlias = tag.GetValue();
          const auto &asgInstances = asg.GetInstances();
          std::unordered_set<std::string> instances;
          for (const auto &i : asgInstances) {
            if (i.GetLifecycleState() == Aws::AutoScaling::Model::LifecycleState::InService
                && i.GetHealthStatus() == "Healthy") {
              instances.insert(i.GetInstanceId());
            }
          }
          if (!instances.empty()) {
            results->emplace(dnsAlias, instances);
          }
        }
      }
    }
  }
  return success;
}

std::shared_ptr<CloudDnsClient> Ec2DnsClient::Create(
    CloudDnsConfig &dnsConfig,
    std::shared_ptr<StatsReceiver> statsReceiver)
{
  Aws::SDKOptions options;
  options.loggingOptions.logLevel = (Aws::Utils::Logging::LogLevel)dnsConfig.log_level;
  options.cryptoOptions.initAndCleanupOpenSSL = false;
  Aws::InitAPI(options);

  Aws::Utils::Logging::InitializeAWSLogging(
      Aws::MakeShared<Aws::Utils::Logging::DefaultLogSystem>(
          "log", (Aws::Utils::Logging::LogLevel)dnsConfig.log_level, dnsConfig.log_path));

  Aws::Client::ClientConfiguration clientConfig;
  clientConfig.connectTimeoutMs = dnsConfig.connect_timeout_ms;
  clientConfig.requestTimeoutMs = dnsConfig.request_timeout_ms;
  clientConfig.region = dnsConfig.region;

  std::shared_ptr<EC2Client> ec2Client;
  std::shared_ptr<AutoScalingClient> asgClient;

  if (!dnsConfig.aws_access_key.empty() && !dnsConfig.aws_secret_key.empty()) {
    Aws::Auth::AWSCredentials creds(
        dnsConfig.aws_access_key,
        dnsConfig.aws_secret_key);
    ec2Client = std::make_shared<EC2Client>(creds, clientConfig);
    asgClient = std::make_shared<AutoScalingClient>(creds, clientConfig);
  }
  else {
    ec2Client = std::make_shared<EC2Client>(clientConfig);
    asgClient = std::make_shared<AutoScalingClient>(clientConfig);
  }

  return std::make_shared<Ec2DnsClient>(
      ec2Client,
      asgClient,
      dnsConfig,
      statsReceiver);
}