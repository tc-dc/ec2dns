#include <aws/core/utils/StringUtils.h>
#include "GceDnsClient.h"

std::shared_ptr<CloudDnsClient> GceDnsClient::Create(CloudDnsConfig &dnsConfig, log_t *logCb, std::shared_ptr<StatsReceiver> statsReceiver) {
  auto clientConfig = CloudDnsClient::InitHttpClient(dnsConfig);
  GoogleApiClient *apiClient;
  GoogleApiClient::TryCreate(dnsConfig, clientConfig, &apiClient);
  return std::make_shared<GceDnsClient>(
      logCb,
      dnsConfig,
      statsReceiver,
      std::unique_ptr<GoogleApiClient>(apiClient));
}

bool GceDnsClient::_GetZones(std::unordered_set<std::string> *zones) {
  auto ret  = m_apiClient->CallApi("https://www.googleapis.com/compute/v1/projects/twitter-ads/zones", HttpMethod::HTTP_GET, JsonValue());
  std::string regionUri = "https://www.googleapis.com/compute/v1/projects/twitter-ads/regions/" + this->m_config.region;
  if (!ret.IsSuccess())
    return false;

  auto& data = ret.GetResult();
  auto items = data->GetArray("items");
  for (size_t i = 0; i < items.GetLength(); i++) {
    auto& zone = items[i];
    if (zone.GetString("region") == regionUri) {
      zones->insert(zone.GetString("name"));
    }
  }
  return true;
}


bool GceDnsClient::_DescribeInstances(const std::string &instanceId, const std::string &ip,
                                      std::vector<Instance> *instances) {

  std::unordered_set<std::string> zones;
  if (!this->_GetZones(&zones))
    return false;

  auto ret = m_apiClient->CallApi("https://www.googleapis.com/compute/v1/projects/twitter-ads/aggregated/instances", Aws::Http::HttpMethod::HTTP_GET, JsonValue());
  if (ret.IsSuccess()) {
    auto& data = ret.GetResult();
    auto items = data->GetObject("items");
    for (auto& kv : items.GetAllObjects()) {
      auto zone = kv.first.substr(6);
      if (zones.find(zone) != zones.end() && kv.second.ValueExists("instances")) {
        auto jsInstances = kv.second.GetArray("instances");
        for (size_t i = 0; i < jsInstances.GetLength(); i++) {
          auto inst = jsInstances[i];
          auto ifaces = inst.GetArray("networkInterfaces");
          if (ifaces.GetLength() > 0) {
            std::string privateIp = ifaces[0].GetString("networkIP");
            std::stringstream hexId;
            hexId << "i-" << std::hex << Aws::Utils::StringUtils::ConvertToInt64(inst.GetString("id").c_str());
            instances->push_back(Instance(
                hexId.str(),
                privateIp,
                zone));
          }
        }
      }
    }
  }
  return true;
}

bool GceDnsClient::_DescribeAutoscalingGroups(
    std::unordered_map<std::string, const std::unordered_set<std::string>> *results) {
  return false;
}