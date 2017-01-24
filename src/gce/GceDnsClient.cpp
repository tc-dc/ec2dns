#include <aws/core/utils/StringUtils.h>
#include "gce/GceDnsClient.h"
#include "gce/InstanceCredentials.h"

#include <istream>
#include <fstream>
#include <sstream>

#include <json/json.h>
#include <glog/logging.h>
#include <googleapis/client/auth/oauth2_authorization.h>
#include <googleapis/client/data/data_reader.h>
#include <googleapis/client/transport/http_transport.h>
#include <googleapis/client/transport/curl_http_transport.h>
#include <google/compute_api/compute_api.h>

using namespace googleapis::client;
using namespace googleapis::util;

std::shared_ptr<CloudDnsClient> GceDnsClient::Create(CloudDnsConfig &dnsConfig, std::shared_ptr<StatsReceiver> statsReceiver) {
  auto config = std::unique_ptr<HttpTransportLayerConfig>(new HttpTransportLayerConfig());
  config->mutable_default_transport_options()->set_cacerts_path("");
  config->mutable_default_transport_options()->set_connect_timeout_ms(dnsConfig.connect_timeout_ms);
  auto factory = new CurlHttpTransportFactory(config.get());
  factory->mutable_request_options()->set_timeout_ms(dnsConfig.request_timeout_ms);
  factory->mutable_request_options()->set_max_retries(3);
  config->ResetDefaultTransportFactory(factory);

  Status status;
  auto httpTransport = config->NewDefaultTransport(&status);
  if (!status.ok()) {
    LOG(ERROR) << "Error configuring curl transport\n" << status.error_message();
    return std::shared_ptr<CloudDnsClient>();
  }
  auto service = std::unique_ptr<google_compute_api::ComputeService>(
      new google_compute_api::ComputeService(httpTransport));

  std::unique_ptr<AuthorizationCredential> creds;
  if (!dnsConfig.credentials_file.empty()) {
    std::ifstream f(dnsConfig.credentials_file);
    if (f.fail()) {
      // File didnt exist
      LOG(ERROR) << "Error loading credentials file " << dnsConfig.credentials_file;
      return std::shared_ptr<CloudDnsClient>();
    }

    Json::Reader r;
    Json::Value root;
    if (!r.parse(f, root)
        || !root.isMember("client_id")
        || !root.isMember("client_secret")
        || !root.isMember("refresh_token")) {
      LOG(ERROR) << "Error parsing json credentials file";
      return std::shared_ptr<CloudDnsClient>();
    }

    auto flow = new googleapis::client::OAuth2AuthorizationFlow(httpTransport);
    flow->mutable_client_spec()->set_client_id(root["client_id"].asString());
    flow->mutable_client_spec()->set_client_secret(root["client_secret"].asString());

    auto oauthCreds = std::unique_ptr<OAuth2Credential>(new OAuth2Credential());
    oauthCreds->set_refresh_token(root["refresh_token"].asString());
    oauthCreds->set_flow(flow);
    creds = std::move(oauthCreds);
  } else {
    creds = std::unique_ptr<InstanceCredentials>(new InstanceCredentials(httpTransport));
  }

  return std::make_shared<GceDnsClient>(
      dnsConfig,
      statsReceiver,
      std::move(service),
      std::move(creds),
      std::move(config)
  );
}

bool GceDnsClient::_GetZones(std::unordered_set<std::string> *zones) {
  std::unique_ptr<google_compute_api::ZonesResource_ListMethodPager> pager(
      this->m_apiClient->get_zones().NewListMethodPager(m_credentials.get(), this->m_config.profile_name));

  std::string regionUri = "https://www.googleapis.com/compute/v1/projects/" + this->m_config.profile_name + "/regions/" + this->m_config.region;
  while(pager->NextPage()) {
    for (const auto& z : pager->data()->get_items()) {
      if (z.get_region() == regionUri) {
        zones->insert(z.get_name().as_string());
      }
    }
  }
  return true;
}


bool GceDnsClient::_DescribeInstances(const std::string &instanceId, const std::string &ip,
                                      std::vector<Instance> *instances) {
  std::unordered_set<std::string> zones;
  this->_GetZones(&zones);

  std::string filter;
  if (!instanceId.empty()) {
    std::istringstream iid(instanceId);
    uint64 iid_val;
    iid >> std::hex >> iid_val;

    std::ostringstream filter_;
    filter_ << "(id eq " << iid_val << ")";
    filter = filter_.str();
  }
  if (!ip.empty()) {
    return false; // can't filter on IP in the API
  }

  for (const auto &z : zones) {
    std::unique_ptr<google_compute_api::InstancesResource_ListMethodPager> pager(
        this->m_apiClient->get_instances().NewListMethodPager(
            m_credentials.get(),
            this->m_config.profile_name,
            z));
    if (!filter.empty()) {
      pager->request()->set_filter(filter);
    }

    while (pager->NextPage()) {
      for (const auto &i : pager->data()->get_items()) {
        const auto ifaces = i.get_network_interfaces();
        if (ifaces.size() > 0) {
          const std::string privateIp = ifaces.get(0).get_network_ip().as_string();
          std::stringstream hexId;
          hexId << "i-" << std::hex << i.get_id();
          instances->push_back(Instance(
              hexId.str(),
              privateIp,
              z));
        }
      }
    }
  }
  return true;
}

bool GceDnsClient::_DescribeAutoscalingGroups(
    std::unordered_map<std::string, const std::unordered_set<std::string>> *results) {
  // TODO: lolol GCE doesnt support tags on things?  Not sure how I'm going to do this...
  return false;
  /*
  std::unordered_set<std::string> zones;
  this->_GetZones(&zones);

  std::unique_ptr<google_compute_api::InstanceGroupsResource_AggregatedListMethodPager> pager(
      this->m_apiClient->get_instance_groups().NewAggregatedListMethodPager(m_credentials.get(), this->m_config.profile_name));

  while(pager->NextPage()) {
    for (const auto& kv : pager->data()->get_items()) {
      auto zone = kv.first.substr(6);
      if (zones.find(zone) == zones.end()) {
        continue;
      }

      for (const auto& ig : kv.second.get_instance_groups()) {
        ig.get_
      }
    }
  }

  return false;
  */
}