#include "gce/GceDnsClient.h"
#include "gce/InstanceCredentials.h"
#include "gce/BoostThreadPoolExecutor.h"
#include "gce/AsyncPager.h"

#include <istream>
#include <fstream>
#include <sstream>

#include <json/json.h>
#include <glog/logging.h>
#include <googleapis/util/executor.h>
#include <googleapis/base/callback-specializations.h>
#include <googleapis/client/auth/oauth2_authorization.h>
#include <googleapis/client/data/data_reader.h>
#include <googleapis/client/transport/http_transport.h>
#include <googleapis/client/transport/curl_http_transport.h>
#include <googleapis/util/status.h>
#include <google/compute_api/compute_api.h>
#include <boost/algorithm/string/predicate.hpp>

using namespace googleapis::client;
using namespace googleapis::util;

std::shared_ptr<CloudDnsClient> GceDnsClient::Create(CloudDnsConfig &dnsConfig, std::shared_ptr<StatsReceiver> statsReceiver) {
  auto config = std::unique_ptr<HttpTransportLayerConfig>(new HttpTransportLayerConfig());
  auto executor = std::unique_ptr<googleapis::thread::Executor>(
      new BoostThreadPoolExecutor(dnsConfig.max_request_pool_size, dnsConfig.max_request_concurrency));
  auto tOpts = config->mutable_default_transport_options();
  tOpts->set_executor(executor.get());
  tOpts->set_cacerts_path("");
  tOpts->set_connect_timeout_ms(dnsConfig.connect_timeout_ms);

  auto factory = new CurlHttpTransportFactory(config.get());
  auto rOpts = factory->mutable_request_options();
  rOpts->set_timeout_ms(dnsConfig.request_timeout_ms);
  rOpts->set_max_retries(3);
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
      std::move(config),
      std::move(executor)
  );
}

template<class REQUEST, class DATA>
bool SafePage(std::unique_ptr<ServiceRequestPager<REQUEST, DATA>>& pager) {
  int retries = 0;
  while(true) {
    auto status = pager->ExecuteNextPage();
    if (status.ok()) {
      return true;
    }

    if (status.error_code() == error::DEADLINE_EXCEEDED && ++retries < 3) {
      LOG(WARNING) << "Request timed out, retrying (" << retries << " of 3)";
      continue;
    }
    else if (status.error_code() != error::OUT_OF_RANGE) {
      LOG(ERROR) << "Error making request " << status.ToString();
    }
    return false;
  }
}

bool GceDnsClient::_GetZones(std::unordered_set<std::string> *zones) {
  if (this->m_zoneCache.TryGet("zones", zones)) {
    return true;
  }

  std::unique_ptr<google_compute_api::ZonesResource_ListMethodPager> pager(
      this->m_apiClient->get_zones().NewListMethodPager(m_credentials.get(), this->m_config.profile_name));

  std::unordered_set<std::string> newZones;
  std::string regionUri = "https://www.googleapis.com/compute/v1/projects/" + this->m_config.profile_name + "/regions/" + this->m_config.region;
  while(SafePage(pager)) {
    for (const auto& z : pager->data()->get_items()) {
      if (z.get_region() == regionUri) {
        newZones.insert(z.get_name().as_string());
      }
    }
  }
  if (newZones.size() != 0) {
    this->m_zoneCache.Insert("zones", newZones);
    *zones = newZones;
    return true;
  } else {
    return false;
  }
}

bool GceDnsClient::_QueryInstanceByIp(const std::string &ip, const std::string &clientAddr, std::string *hostname) {
  if (ip != clientAddr) {
    return false;
  }

  _RefreshInstanceDataImpl();
  return this->_CheckHostCache(ip, hostname);
}


bool GceDnsClient::_DescribeInstances(const std::string &instanceId, const std::string &ip,
                                      std::vector<std::unique_ptr<Instance>> *instances) {
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
    // We don't support filtering on private IP yet, this is handled
    // higher up by just refreshing all instances on a cache miss.
    return false;
  }

  /*
  for (const auto &z : zones) {
    std::unique_ptr<google_compute_api::InstancesResource_ListMethodPager> method(
        this->m_apiClient->get_instances().NewListMethodPager(
            m_credentials.get(),
            this->m_config.profile_name,
            z));
    if (!filter.empty()) {
      method->request()->set_filter(filter);
    }
    while(SafePage(method)) {
      auto r = this->_ProcessInstancesPage(z, *method->data());
      std::move(r.begin(), r.end(), std::back_inserter(*instances));
    }
  }*/
  std::list<ListInstancesPager> futures;
  for (const auto &z : zones) {
    std::unique_ptr<google_compute_api::InstancesResource_ListMethod> method(
      this->m_apiClient->get_instances().NewListMethod(
          m_credentials.get(),
          this->m_config.profile_name,
          z));
    if (!filter.empty()) {
      method->set_filter(filter);
    }
    futures.emplace_back(
        std::move(method), std::bind(&GceDnsClient::_ProcessInstancesPage, this, z, std::placeholders::_1));
  }

  for (auto& f : futures) {
    std::vector<InstanceResult> r = f.GetFuture().get();
    std::move(r.begin(), r.end(), std::back_inserter(*instances));
  }
  return true;
}

std::vector<GceDnsClient::InstanceResult> GceDnsClient::_ProcessInstancesPage(
    const std::string &zone,
    const google_compute_api::InstanceList &r) {
  std::vector<std::unique_ptr<Instance>> ret;
  for (const auto &i : r.get_items()) {
    const auto ifaces = i.get_network_interfaces();
    if (ifaces.size() > 0) {
      const std::string privateIp = ifaces.get(0).get_network_ip().as_string();
      std::stringstream hexId;
      hexId << "i-" << std::hex << i.get_id();

      const auto &metadata = i.get_metadata();
      std::string asgLabel;

      for (const auto &md : metadata.get_items()) {
        if (md.get_key() == this->m_config.asg_dns_tag) {
          asgLabel = md.get_value().as_string();
          break;
        }
      }
      ret.emplace_back(new GceInstance(
          hexId.str(),
          privateIp,
          zone,
          asgLabel
      ));
    }
  }
  return ret;
}

void GceDnsClient::_AfterRefresh() {
  this->m_zoneCache.Trim();
}

void GceDnsClient::_RefreshAutoscalerDataImpl(const std::vector<std::unique_ptr<Instance>>& instances) {
  std::unordered_map<std::string, std::vector<std::string>> asgMaps;
  for (const auto& instance : instances) {
    const auto* gceInstance = static_cast<const GceInstance*>(instance.get());
    if (gceInstance->HasAsgLabel()) {
      const auto& value = gceInstance->GetAsgLabel();
      const auto& asg = asgMaps.find(value);
      if (asg == asgMaps.end()) {
        asgMaps[value] = std::vector<std::string>();
      }
      asgMaps[value].push_back(gceInstance->GetPrivateIpAddress());
    }
  }

  auto expiresOn = std::chrono::steady_clock::now() + std::chrono::seconds(10 * 60);
  for (const auto& kv : asgMaps) {
    this->m_asgCache.Insert(kv.first, kv.second, expiresOn);
  }

  this->m_asgCache.Trim();
}

bool GceDnsClient::_DescribeAutoscalingGroups(
    std::unordered_map<std::string, const std::unordered_set<std::string>> *results) {
  // TODO: lolol GCE doesnt support tags on things
  return false;
}