#include <fstream>

#include "Ec2DnsClient.h"
#include "dlz_minimal.h"
#include "aws/core/utils/json/JsonSerializer.h"


bool TryLoadEc2DnsConfig(Aws::String file, Ec2DnsConfig *config) {
  std::ifstream f(file);
  if(f.fail()) {
    // File didnt exist
    return false;
  }

  Aws::Utils::Json::JsonValue root(f);
  if(!root.WasParseSuccessful()) {
    // Not valid json
    return false;
  }

  if(root.ValueExists("aws_access_key")) {
    config->aws_access_key = root.GetString("aws_access_key");
  }
  if(root.ValueExists("aws_secret_key")) {
    config->aws_secret_key = root.GetString("aws_secret_key");
  }

  return true;
}


bool Ec2DnsClient::ResolveInstanceIp(Aws::String instanceId, Aws::String *ip) {
  this->m_callbacks.log(ISC_LOG_WARNING, "Querying name %s", instanceId.c_str());

  Aws::EC2::Model::DescribeInstancesRequest req;
  req.AddInstanceIds(instanceId);
  auto ret = this->m_ec2Client->DescribeInstances(req);
  this->m_callbacks.log(ISC_LOG_WARNING, "Request complete");
  auto success = ret.IsSuccess();
  if(!success) {
    return false;
  }

  auto reservs = ret.GetResult().GetReservations();
  for (auto &r : reservs) {
    auto instances = r.GetInstances();
    for (auto &i : instances) {
      *ip = i.GetPrivateIpAddress();
      this->m_callbacks.log(ISC_LOG_WARNING, "Found IP");
      return true;
    }
  }
  return false;
}

bool Ec2DnsClient::IsAwsZone(const char *zone) {
  this->m_callbacks.log(ISC_LOG_WARNING, "Querying AWS zone %s", zone);
  return (strcasecmp(this->m_zoneName.c_str(), zone) == 0);
}