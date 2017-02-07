#ifndef GCEDNS_EC2DNSCLIENT_H
#define GCEDNS_EC2DNSCLIENT_H

#include <future>
#include <unordered_set>

#include <googleapis/util/executor.h>
#include <googleapis/client/transport/http_authorization.h>
#include <googleapis/client/transport/http_transport.h>
#include <google/compute_api/compute_api.h>

#include "CloudDnsClient.h"
#include "gce/AsyncPager.h"

class GceInstance : public Instance {
 public:
  GceInstance(
      const std::string &instanceId,
      const std::string &privateIp,
      const std::string &az,
      const std::string &asgLabel
  )
    : Instance(instanceId, privateIp, az), m_asgLabel(asgLabel)
  { }

  bool HasAsgLabel() const { return !m_asgLabel.empty(); }
  const std::string& GetAsgLabel() const { return m_asgLabel; }

 private:
  std::string m_asgLabel;
};

class GceDnsClient : public CloudDnsClient {
 public:
  GceDnsClient(
      const CloudDnsConfig config,
      std::shared_ptr<StatsReceiver> statsReceiver,
      std::unique_ptr<google_compute_api::ComputeService> apiClient,
      std::unique_ptr<googleapis::client::AuthorizationCredential> credentials,
      std::unique_ptr<googleapis::client::HttpTransportLayerConfig> transportConfig,
      std::unique_ptr<googleapis::thread::Executor> executor
  ) : CloudDnsClient(config, statsReceiver),
      m_transportConfig(std::move(transportConfig)),
      m_apiClient(std::move(apiClient)),
      m_credentials(std::move(credentials)),
      m_executor(std::move(executor)),
      m_zoneCache("zones", statsReceiver, 60 * 60 * 5),
      m_config(config) {
  }

  static std::shared_ptr<CloudDnsClient> Create(CloudDnsConfig &dnsConfig,
                                                std::shared_ptr<StatsReceiver> statsReceiver);

 protected:
  virtual void _RefreshAutoscalerDataImpl(const std::vector<std::unique_ptr<Instance>>& instances) override;
  virtual void _AfterRefresh() override;

  virtual bool _DescribeInstances(
      const std::string &instanceId,
      const std::string &ip,
      std::vector<std::unique_ptr<Instance>> *instances) override;
  virtual bool _DescribeAutoscalingGroups(
      std::unordered_map<std::string,
      const std::unordered_set<std::string>> *results) override;

  bool _GetZones(std::unordered_set<std::string> *zones);
  virtual bool _QueryInstanceByIp(const std::string& ip, const std::string& clientAddr, std::string *hostname) override;

 private:
  typedef std::unique_ptr<Instance> InstanceResult;
  typedef AsyncPager<
      google_compute_api::InstancesResource_ListMethod,
      google_compute_api::InstanceList,
      InstanceResult> ListInstancesPager;

  std::vector<InstanceResult> _ProcessInstancesPage(const std::string& zone, const google_compute_api::InstanceList& r);

  std::unique_ptr<googleapis::client::HttpTransportLayerConfig> m_transportConfig;
  std::unique_ptr<google_compute_api::ComputeService> m_apiClient;
  std::unique_ptr<googleapis::client::AuthorizationCredential> m_credentials;
  std::unique_ptr<googleapis::thread::Executor> m_executor;
  Cache<std::unordered_set<std::string>> m_zoneCache;
  CloudDnsConfig m_config;
};

#endif