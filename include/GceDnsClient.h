#ifndef GCEDNS_EC2DNSCLIENT_H
#define GCEDNS_EC2DNSCLIENT_H

#include <unordered_set>

#include "CloudDnsClient.h"
#include "GoogleApiClient.h"

class GceDnsClient : public CloudDnsClient {
public:
    GceDnsClient(
        log_t *logCb,
        const CloudDnsConfig config,
        std::shared_ptr<StatsReceiver> statsReceiver,
        std::unique_ptr<GoogleApiClient> apiClient
    ) : CloudDnsClient(logCb, config, statsReceiver),
        m_apiClient(std::move(apiClient)),
        m_config(config)
    {
    }

    static std::shared_ptr<CloudDnsClient> Create(CloudDnsConfig &dnsConfig, log_t *logCb, std::shared_ptr<StatsReceiver> statsReceiver);

protected:
    virtual bool _DescribeInstances(const std::string& instanceId, const std::string& ip, std::vector<Instance> *instances);
    virtual bool _DescribeAutoscalingGroups(std::unordered_map<std::string, const std::unordered_set<std::string>> *results);

    bool _GetZones(std::unordered_set<std::string> *zones);

private:
    std::unique_ptr<GoogleApiClient> m_apiClient;
    CloudDnsConfig m_config;
};

#endif