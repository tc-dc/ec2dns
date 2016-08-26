#include <fstream>
#include <boost/regex.hpp>

#include "Ec2DnsClient.h"
#include "dlz_minimal.h"
#include "aws/core/utils/json/JsonSerializer.h"

using namespace std::placeholders;

bool Ec2DnsConfig::TryLoad(const std::string& file) {
#define TryLoadString(key) if (root.ValueExists(#key)) { this->key = root.GetString(#key); }
#define TryLoadInteger(key) if (root.ValueExists(#key)) { this->key = root.GetInteger(#key); }

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

  TryLoadString(aws_access_key)
  TryLoadString(aws_secret_key)
  TryLoadInteger(log_level)
  TryLoadString(log_path)
  TryLoadInteger(num_asg_records)
  TryLoadString(asg_dns_tag)

  if (root.ValueExists("requestTimeoutMs")) {
    this->client_config.requestTimeoutMs = root.GetInteger("requestTimeoutMs");
  }
  else {
    this->client_config.requestTimeoutMs = 1000;
  }
  if (root.ValueExists("connectTimeoutMs")) {
    this->client_config.connectTimeoutMs = root.GetInteger("connectTimeoutMs");
  }
  else {
    this->client_config.connectTimeoutMs = 1000;
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
    this->client_config.region = regionEnum;
  }
  if (root.ValueExists("authentication_region")) {
    this->client_config.authenticationRegion = root.GetString("authentication_region");
  }
  if (root.ValueExists("endpoint_override")) {
    this->client_config.endpointOverride = root.GetString("endpoint_override");
  }
  if (root.ValueExists("use_ssl")) {
    this->client_config.scheme =
        root.GetBool("use_ssl") ? Aws::Http::Scheme::HTTPS : Aws::Http::Scheme::HTTP;
  }
  TryLoadString(instance_regex)
  TryLoadString(account_name)
  TryLoadInteger(request_batch_size)
  return true;
}

bool Ec2DnsClient::_DescribeInstances(
    const std::string &instanceId,
    const std::string &ip,
    Aws::Vector<Aws::EC2::Model::Instance> *instances) {

  Aws::EC2::Model::DescribeInstancesRequest req;
  req.SetMaxResults(this->m_config.request_batch_size);
  if (instanceId.empty() && ip.empty()) {
    this->m_log(ISC_LOG_INFO, "ec2dns - Getting all instances");
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
      instances->insert(
          instances->end(),
          resInstances.begin(),
          resInstances.end());
    }
  }
  return true;
}

bool Ec2DnsClient::_QueryInstanceById(const std::string &instanceId, std::string *ip) {
  this->m_log(
      ISC_LOG_INFO, "ec2dns - Querying name %s", instanceId.c_str());

  Aws::Vector<Aws::EC2::Model::Instance> instances;
  bool success = this->_DescribeInstances(instanceId, "", &instances);
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

const std::string Ec2DnsClient::_GetHostname(const Aws::EC2::Model::Instance& instance) {
  const auto& regionCode = this->_GetRegionCode(this->m_config.client_config.region);
  const auto& az = instance.GetPlacement().GetAvailabilityZone();
  const auto& account = this->m_config.account_name;
  auto instanceId = instance.GetInstanceId().substr(2);
  std::ostringstream oss;
  oss << regionCode << az[az.length() - 1] << "-" << account << "-" << instanceId
      << "." << this->m_config.zone_name << ".";
  return oss.str();
}

bool Ec2DnsClient::_QueryInstanceByIp(const std::string &ip, std::string *hostname) {
  Aws::Vector<Aws::EC2::Model::Instance> instances;
  bool success = this->_DescribeInstances("", ip, &instances);
  if (success && instances.size() > 0) {
    auto instance = instances[0];
    *hostname = this->_GetHostname(instance);
    return true;
  }
  if (!success) {
    return false;
  }
  this->m_log(
      ISC_LOG_WARNING,
      "ec2dns - Unable to resolve hostname for ip %s because it was not found,",
      ip.c_str()
  );
  return false;
}

bool Ec2DnsClient::_CheckHostCache(const std::string &instanceId, std::string *ip) {
  return this->m_hostCache.TryGet(instanceId, ip);
}

bool Ec2DnsClient::_Resolve(
    const std::string &key,
    const std::string &clientAddr,
    const std::function<bool(const std::string&, std::string*)> valueFactory,
    std::string *value) {
  if (key.empty()) {
    return false;
  }
  if (this->_CheckHostCache(key, value)) {
    return true;
  }
  if (this->m_throttler->IsRequestThrottled(clientAddr, key)) {
    return false;
  }
  this->m_throttler->OnMiss(key, clientAddr);
  if (valueFactory(key, value)) {
    this->m_hostCache.Insert(key, *value);
    return true;
  }
  return false;
}

bool Ec2DnsClient::TryResolveIp(const std::string &instanceId, const std::string& clientAddr, std::string *ip) {
  this->m_lookupRequests->Increment();
  return this->_Resolve(
      instanceId,
      clientAddr,
      std::bind(&Ec2DnsClient::_QueryInstanceById, this, _1, _2),
      ip);
}

bool Ec2DnsClient::TryResolveHostname(const std::string &ip, const std::string &clientAddr, std::string *hostname) {
  this->m_reverseLookupRequests->Increment();
  return this->_Resolve(
      ip,
      clientAddr,
      std::bind(&Ec2DnsClient::_QueryInstanceByIp, this, _1, _2),
      hostname);
}

bool Ec2DnsClient::TryResolveAutoscaler(const std::string &name, const std::string &clientAddr, std::vector<std::string> *nodes) {
  this->m_autoscalerRequests->Increment();
  return this->m_asgCache.TryGet(name, nodes);
}

void Ec2DnsClient::_RefreshInstanceData() {
  while (true) {
    this->_RefreshInstanceDataImpl();
    this->m_throttler->Trim();
    std::this_thread::sleep_for(
        std::chrono::seconds(this->m_config.refresh_interval));
  }
}

void Ec2DnsClient::_RefreshInstanceDataImpl() {
  Aws::Vector<Aws::EC2::Model::Instance> instances;
  bool success = this->_DescribeInstances("", "", &instances);
  if (not success) {
    this->m_log(ISC_LOG_ERROR, "ec2dns - Unable to refresh cache.");
    return;
  }
  this->_RefreshAutoscalerDataImpl(instances);

  {
    auto&& lock = this->m_hostCache.GetLock();
    this->m_hostCache.Trim();

    auto expiresOn = std::chrono::steady_clock::now() + std::chrono::seconds(this->m_config.instance_timeout);
    for (const auto& it : instances) {
      this->m_hostCache.InsertNoLock(it.GetInstanceId(), it.GetPrivateIpAddress(), expiresOn);
      this->m_hostCache.InsertNoLock(it.GetPrivateIpAddress(), this->_GetHostname(it), expiresOn);
    }
    UNUSED(lock);
  }
  this->m_log(ISC_LOG_INFO, "ec2dns - Refreshed cache with %d instances", instances.size());
}

void Ec2DnsClient::_RefreshAutoscalerDataImpl(const Aws::Vector<Aws::EC2::Model::Instance>& instances) {
  auto req = Aws::AutoScaling::Model::DescribeAutoScalingGroupsRequest();
  std::vector<Aws::AutoScaling::Model::DescribeAutoScalingGroupsResult> results;
  bool success = this->_CallApi<
      Aws::AutoScaling::Model::DescribeAutoScalingGroupsRequest,
      Aws::AutoScaling::Model::DescribeAutoScalingGroupsResult,
      Aws::AutoScaling::AutoScalingErrors
  >("DescribeAutoScalingGroups", req,
    std::bind(&AutoScalingClient::DescribeAutoScalingGroups, this->m_asgClient, _1), &results);

  if (!success) {
    return;
  }

  std::unordered_map<std::string, std::string> instanceToIpLookup;
  for (const auto &i : instances) {
    instanceToIpLookup[i.GetInstanceId()] = i.GetPrivateIpAddress();
  }

  auto expiresOn = std::chrono::steady_clock::now() + std::chrono::seconds(10 * 60);
  for (const auto &resp : results) {
    for (const auto &asg : resp.GetAutoScalingGroups()) {
      for (const auto &tag : asg.GetTags()) {
        if (tag.GetKey() == this->m_config.asg_dns_tag) {
          const auto& dnsAlias = tag.GetValue();
          const auto& asgInstances = asg.GetInstances();
          std::vector<std::string> instanceIps;
          for (const auto &i : asgInstances) {
            if (i.GetLifecycleState() == Aws::AutoScaling::Model::LifecycleState::InService
                && i.GetHealthStatus() == "Healthy") {
              auto instanceInfo = instanceToIpLookup.find(i.GetInstanceId());
              if (instanceInfo != instanceToIpLookup.end()) {
                instanceIps.push_back(instanceInfo->second);
              }
            }
          }
          this->m_asgCache.Insert(dnsAlias, instanceIps, expiresOn);
        }
      }
    }
  }
  this->m_asgCache.Trim();
}