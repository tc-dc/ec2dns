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
  if(root.ValueExists("log_level")) {
    config->log_level = root.GetInteger("log_level");
  }
  else {
    config->log_level = 0; //Off
  }
  if(root.ValueExists("log_path")) {
    config->log_path = root.GetString("log_path");
  }
  else {
    config->log_path = "ec2_dns_aws_";
  }
  if(root.ValueExists("requestTimeoutMs")) {
    config->client_config.requestTimeoutMs = root.GetInteger("requestTimeoutMs");
  }
  else {
    config->client_config.requestTimeoutMs = 1000;
  }
  if(root.ValueExists("connectTimeoutMs")) {
    config->client_config.connectTimeoutMs = root.GetInteger("connectTimeoutMs");
  }
  else {
    config->client_config.connectTimeoutMs = 1000;
  }
  if(root.ValueExists("region")) {
    auto region = root.GetString("region");
    auto regionEnum = Aws::Region::US_EAST_1;
#define MAYBE_SET_REGION(str, value) if(region == str) { regionEnum = Aws::Region::value; }

    MAYBE_SET_REGION("us-west-1", US_WEST_1)
    MAYBE_SET_REGION("us-west-2", US_WEST_2)
    MAYBE_SET_REGION("eu-west-1", EU_WEST_1)
    MAYBE_SET_REGION("eu-central-1", EU_CENTRAL_1)
    MAYBE_SET_REGION("ap-southeast-1", AP_SOUTHEAST_1)
    MAYBE_SET_REGION("ap-southeast-2", AP_SOUTHEAST_2)
    MAYBE_SET_REGION("ap-northeast-1", AP_NORTHEAST_1)
    MAYBE_SET_REGION("sa-east-1", SA_EAST_1)
    config->client_config.region = regionEnum;
  }
  if(root.ValueExists("authenticationRegion")) {
    config->client_config.authenticationRegion = root.GetString("authenticationRegion");
  }
  if(root.ValueExists("endpointOverride")) {
    config->client_config.endpointOverride = root.GetString("endpointOverride");
  }
  if(root.ValueExists("use_ssl")) {
    config->client_config.scheme =
      root.GetBool("use_ssl") ? Aws::Http::Scheme::HTTPS : Aws::Http::Scheme::HTTP;
  }
  return true;
}


bool Ec2DnsClient::ResolveInstanceIp(Aws::String instanceId, Aws::String *ip) {
  this->m_callbacks.log(
    ISC_LOG_INFO, "ec2dns - Querying name %s", instanceId.c_str());

  Aws::EC2::Model::DescribeInstancesRequest req;
  req.AddInstanceIds(instanceId);
  auto ret = this->m_ec2Client->DescribeInstances(req);
  this->m_callbacks.log(ISC_LOG_INFO, "ec2dns - API Request complete");
  auto success = ret.IsSuccess();
  if(!success) {
    auto errorMessage = ret.GetError().GetMessage();
    this->m_callbacks.log(
      ISC_LOG_ERROR,
      "ec2dns - API request DescribeInstances failed with error: %s",
      errorMessage.c_str());
    return false;
  }

  auto reservs = ret.GetResult().GetReservations();
  for (auto &r : reservs) {
    auto instances = r.GetInstances();
    for (auto &i : instances) {
      *ip = i.GetPrivateIpAddress();
      this->m_callbacks.log(ISC_LOG_INFO, "Found IP");
      return true;
    }
  }
  this->m_callbacks.log(
    ISC_LOG_WARNING,
    "ec2dns - Unable to resolve instance %s because it was not found.",
    instanceId.c_str());
  return false;
}