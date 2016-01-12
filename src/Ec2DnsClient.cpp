#include <fstream>

#include "Ec2DnsClient.h"
#include "dlz_minimal.h"
#include "aws/core/utils/json/JsonSerializer.h"

bool TryLoadEc2DnsConfig(Aws::String file, Ec2DnsConfig *config) {
  std::ifstream f(file);
  if (f.fail()) {
    // File didnt exist
    return false;
  }

  Aws::Utils::Json::JsonValue root(f);
  if (!root.WasParseSuccessful()) {
    // Not valid json
    return false;
  }

  if (root.ValueExists("aws_access_key")) {
    config->aws_access_key = root.GetString("aws_access_key");
  }
  if (root.ValueExists("aws_secret_key")) {
    config->aws_secret_key = root.GetString("aws_secret_key");
  }
  if (root.ValueExists("log_level")) {
    config->log_level = root.GetInteger("log_level");
  }
  else {
    config->log_level = 0; //Off
  }
  if (root.ValueExists("log_path")) {
    config->log_path = root.GetString("log_path");
  }
  else {
    config->log_path = "ec2_dns_aws_";
  }
  if (root.ValueExists("requestTimeoutMs")) {
    config->client_config.requestTimeoutMs = root.GetInteger("requestTimeoutMs");
  }
  else {
    config->client_config.requestTimeoutMs = 1000;
  }
  if (root.ValueExists("connectTimeoutMs")) {
    config->client_config.connectTimeoutMs = root.GetInteger("connectTimeoutMs");
  }
  else {
    config->client_config.connectTimeoutMs = 1000;
  }
  if (root.ValueExists("region")) {
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
  if (root.ValueExists("authentication_region")) {
    config->client_config.authenticationRegion = root.GetString("authentication_region");
  }
  if (root.ValueExists("endpoint_override")) {
    config->client_config.endpointOverride = root.GetString("endpoint_override");
  }
  if (root.ValueExists("use_ssl")) {
    config->client_config.scheme =
        root.GetBool("use_ssl") ? Aws::Http::Scheme::HTTPS : Aws::Http::Scheme::HTTP;
  }
  if (root.ValueExists("instance_regex")) {
    config->instance_regex = root.GetString("instance_regex");
  }

  return true;
}

bool Ec2DnsClient::_DescribeInstances(
    const std::string &instanceId,
    Aws::Vector<Aws::EC2::Model::Instance> *instances) {

  Aws::EC2::Model::DescribeInstancesRequest req;
  if (instanceId.empty()) {
    this->m_log(ISC_LOG_INFO, "ec2dns - Getting all instances");
  }
  else {
    req.AddInstanceIds(instanceId);
  }

  std::vector<Aws::EC2::Model::Instance> allInstances;
  std::string nextToken;
  do {
    auto ret = this->m_ec2Client->DescribeInstances(req);
    this->m_log(ISC_LOG_INFO, "ec2dns - API Request complete");
    if (!ret.IsSuccess()) {
      auto errorMessage = ret.GetError().GetMessage();
      this->m_log(
          ISC_LOG_ERROR,
          "ec2dns - API request DescribeInstances failed with error: %s",
          errorMessage.c_str());
      return false;
    }

    auto result = ret.GetResult();
    auto reservs = result.GetReservations();
    for (auto &r : reservs) {
      auto resInstances = r.GetInstances();
      allInstances.insert(
          allInstances.end(),
          resInstances.begin(),
          resInstances.end());
    }
    nextToken = result.GetNextToken();
    req.SetNextToken(nextToken);
  } while (!nextToken.empty());

  *instances = allInstances;
  return true;
}

bool Ec2DnsClient::_QueryInstance(const std::string &instanceId, std::string *ip) {
  this->m_log(
      ISC_LOG_INFO, "ec2dns - Querying name %s", instanceId.c_str());

  Aws::Vector<Aws::EC2::Model::Instance> instances;
  bool success = this->_DescribeInstances(instanceId, &instances);
  if (success && instances.size() > 0) {
    *ip = instances[0].GetPrivateIpAddress();
    return true;
  }
  if (!success) {
    return false;
  }
  this->m_log(
      ISC_LOG_WARNING,
      "ec2dns - Unable to resolve instance %s because it was not found.",
      instanceId.c_str());
  return false;
}

bool Ec2DnsClient::_CheckCache(const std::string &instanceId, std::string *ip) {
  std::lock_guard<std::mutex> lock(this->m_cacheLock);
  auto found = this->m_cache.find(instanceId);
  if (found == this->m_cache.end()) {
    this->m_stats.Miss();
    return false;
  }
  else {
    *ip = found->second.GetIp();
    this->m_stats.Hit();
    return true;
  }
}

void Ec2DnsClient::_InsertCache(
    const std::string &instanceId,
    const std::string &ip) {
  auto expiresOn = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  this->_InsertCache(instanceId, ip, expiresOn);
}

void Ec2DnsClient::_InsertCache(
    const std::string &instanceId,
    const std::string &ip,
    const std::chrono::time_point<std::chrono::steady_clock> expiresOn) {

  std::lock_guard<std::mutex> lock(this->m_cacheLock);
  this->m_cache[instanceId] = CacheEntry(ip, expiresOn);
}

bool Ec2DnsClient::ResolveInstanceIp(const Aws::String &instanceId, Aws::String *ip) {
  if (instanceId.empty()) {
    return false;
  }
  if (this->_CheckCache(instanceId, ip)) {
    return true;
  }
  if (this->_QueryInstance(instanceId, ip)) {
    this->_InsertCache(instanceId, *ip);
    return true;
  }
  return false;
}

void Ec2DnsClient::_RefreshInstanceData() {
  while (true) {
    this->_RefreshInstanceDataImpl();
    std::this_thread::sleep_for(
        std::chrono::seconds(this->m_config.refresh_interval));
  }
}

void Ec2DnsClient::_RefreshInstanceDataImpl() {
  Aws::Vector<Aws::EC2::Model::Instance> instances;
  bool success = this->_DescribeInstances("", &instances);
  if (not success) {
    this->m_log(ISC_LOG_ERROR, "ec2dns - Unable to refresh cache.");
  }

  {
    std::lock_guard<std::mutex>(this->m_cacheLock);
    auto expiresOn = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    std::vector<Aws::String> toDelete;
    for (auto it = this->m_cache.begin(); it != this->m_cache.end(); ++it) {
      if (!it->second.IsValid()) {
        toDelete.push_back(it->first);
      }
    }
    for (auto it = toDelete.begin(); it != toDelete.end(); ++it) {
      this->m_cache.erase(*it);
    }
    for (auto it = instances.begin(); it != instances.end(); ++it) {
      this->_InsertCache(it->GetInstanceId(), it->GetPrivateIpAddress(), expiresOn);
    }
  }
  this->m_log(ISC_LOG_INFO, "ec2dns - Refreshed cache with %d instances", instances.size());
}