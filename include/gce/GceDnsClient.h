#ifndef GCEDNS_EC2DNSCLIENT_H
#define GCEDNS_EC2DNSCLIENT_H

#include <unordered_set>

#include <google/compute_api/compute_api.h>

#include "CloudDnsClient.h"

class GceDnsClient : public CloudDnsClient {
 public:
  GceDnsClient(
      log_t *logCb,
      const CloudDnsConfig config,
      std::shared_ptr<StatsReceiver> statsReceiver,
      std::unique_ptr<google_compute_api::ComputeService> apiClient,
      std::shared_ptr<googleapis::client::AuthorizationCredential> credentials
  ) : CloudDnsClient(logCb, config, statsReceiver),
      m_apiClient(std::move(apiClient)),
      m_credentials(credentials),
      m_config(config) {
  }

  static std::shared_ptr<CloudDnsClient> Create(CloudDnsConfig &dnsConfig,
                                                log_t *logCb,
                                                std::shared_ptr<StatsReceiver> statsReceiver);

 protected:
  virtual bool _DescribeInstances(const std::string &instanceId,
                                  const std::string &ip,
                                  std::vector<Instance> *instances);
  virtual bool _DescribeAutoscalingGroups(std::unordered_map<std::string,
                                                             const std::unordered_set<std::string>> *results);

  bool _GetZones(std::unordered_set<std::string> *zones);

 private:
  std::unique_ptr<google_compute_api::ComputeService> m_apiClient;
  std::shared_ptr<googleapis::client::AuthorizationCredential> m_credentials;
  CloudDnsConfig m_config;
};

#endif