#include <json/json.h>
#include <fstream>
#include <boost/regex.hpp>
#include <aws/core/client/ClientConfiguration.h>

#include "CloudDnsClient.h"
#include "dlz_minimal.h"
#include "aws/core/Aws.h"
#include "aws/core/Region.h"
#include "aws/core/utils/json/JsonSerializer.h"

using namespace std::placeholders;

bool CloudDnsConfig::TryLoad(const std::string& file) {
#define TryLoadString(key) if (root.isMember(#key)) { this->key = root[#key].asString(); }
#define TryLoadInteger(key) if (root.isMember(#key)) { this->key = root[#key].asInt(); }

  std::ifstream f(file);
  if (f.fail()) {
    // File didnt exist
    return false;
  }

  Json::Reader r;
  Json::Value root;
  if (!r.parse(f, root)) {
    return false;
  }

  TryLoadString(provider)
  TryLoadString(aws_access_key)
  TryLoadString(aws_secret_key)
  TryLoadInteger(log_level)
  TryLoadString(log_path)
  TryLoadInteger(num_asg_records)
  TryLoadString(asg_dns_tag)
  TryLoadString(credentials_file)

  if (root.isMember("requestTimeoutMs")) {
    this->request_timeout_ms = root["requestTimeoutMs"].asInt();
  }
  if (root.isMember("connectTimeoutMs")) {
    this->connect_timeout_ms = root["connectTimeoutMs"].asInt();
  }

  if (root.isMember("region")) {
    auto region = root["region"].asString();
    std::string region_code;
#define MAYBE_SET_REGION(str, code) if(region == str) { region_code = code; }

    MAYBE_SET_REGION(Aws::Region::US_EAST_1, "ue1")
    MAYBE_SET_REGION(Aws::Region::US_WEST_1, "uw1")
    MAYBE_SET_REGION(Aws::Region::US_WEST_2, "uw2")
    MAYBE_SET_REGION(Aws::Region::AP_NORTHEAST_1, "an1")
    MAYBE_SET_REGION(Aws::Region::AP_NORTHEAST_2, "an2")
    MAYBE_SET_REGION(Aws::Region::AP_SOUTHEAST_1, "as1")
    MAYBE_SET_REGION(Aws::Region::AP_SOUTHEAST_2, "as2")
    MAYBE_SET_REGION(Aws::Region::EU_WEST_1, "ew1")
    MAYBE_SET_REGION(Aws::Region::EU_CENTRAL_1, "ec1")
    MAYBE_SET_REGION(Aws::Region::SA_EAST_1, "se1")

    this->region = region;
    this->region_code = region_code;
  } else {
    this->region = Aws::Region::US_EAST_1;
    this->region_code = "ue1";
  }
  TryLoadString(region_code)

  TryLoadString(instance_regex)
  TryLoadString(account_name)
  TryLoadString(profile_name)
  TryLoadInteger(request_batch_size)
  TryLoadInteger(refresh_interval)
  return true;
}

void CloudDnsClient::_RefreshInstanceData() {
  while (true) {
    this->_RefreshInstanceDataImpl();
    this->m_throttler->Trim();
    std::this_thread::sleep_for(
        std::chrono::seconds(this->m_config.refresh_interval));
  }
}

bool CloudDnsClient::_QueryInstanceById(const std::string &instanceId, std::string *ip) {
  VLOG(0) << "Querying name " << instanceId.c_str();

  std::vector<Instance> instances;
  bool success = this->_DescribeInstances(instanceId, "", &instances);
  if (success && instances.size() > 0) {
    *ip = instances[0].GetPrivateIpAddress();
    return true;
  }
  if (!success) {
    return false;
  }
  LOG(WARNING) << "Unable to resolve instance " << instanceId << " because it was not found.";
  return false;
}

const std::string CloudDnsClient::_GetHostname(const Instance& instance) {
  const auto& regionCode = this->m_config.region_code;
  const auto& az = instance.GetZone();
  const auto& account = this->m_config.account_name;
  auto instanceId = instance.GetInstanceId().substr(2);
  std::ostringstream oss;
  oss << regionCode << az[az.length() - 1] << "-" << account << "-" << instanceId
      << "." << this->m_config.zone_name << ".";
  return oss.str();
}

bool CloudDnsClient::_QueryInstanceByIp(const std::string &ip, std::string *hostname) {
  std::vector<Instance> instances;
  bool success = this->_DescribeInstances("", ip, &instances);
  if (success && instances.size() > 0) {
    auto instance = instances[0];
    *hostname = this->_GetHostname(instance);
    return true;
  }
  if (!success) {
    return false;
  }
  LOG(WARNING) << "Unable to resolve hostname for ip " << ip << " because it was not found";
  return false;
}

bool CloudDnsClient::_CheckHostCache(const std::string &instanceId, std::string *ip) {
  return this->m_hostCache.TryGet(instanceId, ip);
}

bool CloudDnsClient::_Resolve(
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

bool CloudDnsClient::TryResolveIp(const std::string &instanceId, const std::string& clientAddr, std::string *ip) {
  this->m_lookupRequests->Increment();
  return this->_Resolve(
      instanceId,
      clientAddr,
      std::bind(&CloudDnsClient::_QueryInstanceById, this, _1, _2),
      ip);
}

bool CloudDnsClient::TryResolveHostname(const std::string &ip, const std::string &clientAddr, std::string *hostname) {
  this->m_reverseLookupRequests->Increment();
  return this->_Resolve(
      ip,
      clientAddr,
      std::bind(&CloudDnsClient::_QueryInstanceByIp, this, _1, _2),
      hostname);
}

bool CloudDnsClient::TryResolveAutoscaler(const std::string &name, const std::string &clientAddr, std::vector<std::string> *nodes) {
  this->m_autoscalerRequests->Increment();
  return this->m_asgCache.TryGet(name, nodes);
}

void CloudDnsClient::_RefreshInstanceDataImpl() {
  std::vector<Instance> instances;
  bool success = this->_DescribeInstances("", "", &instances);
  if (!success) {
    LOG(ERROR) << "Unable to refresh cache.";
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
  LOG(INFO) << "Refreshed cache with " << instances.size() << " instances";
}

void CloudDnsClient::_RefreshAutoscalerDataImpl(const std::vector<Instance>& instances) {
  std::unordered_map<std::string, const std::unordered_set<std::string>> results;
  bool success = this->_DescribeAutoscalingGroups(&results);

  if (!success) {
    return;
  }

  std::unordered_map<std::string, std::string> instanceToIpLookup;
  for (const auto &i : instances) {
    instanceToIpLookup[i.GetInstanceId()] = i.GetPrivateIpAddress();
  }

  auto expiresOn = std::chrono::steady_clock::now() + std::chrono::seconds(10 * 60);
  for (auto& entry : results) {
    const auto &dnsAlias = entry.first;
    std::vector<std::string> instanceIps;
    for (auto& instance : entry.second) {
      auto instanceIp = instanceToIpLookup.find(instance);
      if (instanceIp != instanceToIpLookup.end()) {
        instanceIps.push_back(instanceIp->second);
      }
    }
    this->m_asgCache.Insert(dnsAlias, instanceIps, expiresOn);
  }
  this->m_asgCache.Trim();
}