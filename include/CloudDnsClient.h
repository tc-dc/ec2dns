//
// Created by Steve Niemitz on 1/7/16.
//

#ifndef AWSDNS_EC2DNSCLIENT_H
#define AWSDNS_EC2DNSCLIENT_H

#include "dlz_minimal.h"
#include "Cache.h"
#include "Stats.h"
#include "RequestThrottler.h"
#include "aws/core/utils/json/JsonSerializer.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/regex.hpp>

using namespace std::chrono;

class CloudDnsClient;
class HostMatcher;
class ReverseLookupHelper;

struct DlzCallbacks {
  log_t *log;
  dns_sdlz_putrr_t *putrr;
  dns_sdlz_putnamedrr_t *putnamedrr;
};

struct dlz_state {
    std::shared_ptr<CloudDnsClient> client;
    std::unique_ptr<HostMatcher> matcher;
    std::unique_ptr<ReverseLookupHelper> rl_helper;
    std::unique_ptr<StatsServer> stats_server;
    std::shared_ptr<StatsReceiver> stats_receiver;
    std::string soa_data;
    std::string zone_name;
    std::string autoscaler_zone_name;
    size_t num_asg_records;
    DlzCallbacks callbacks;
};


#define DEFAULT_INSTANCE_REGEX "^(?<region>[a-z]{2}\\d)(?<zone>[a-z])-(?<account>\\w+)-(?<instanceId>\\w*)$"

class CloudDnsConfig {
public:
    CloudDnsConfig(const std::string& accountName, const std::string& vpcCidr, const std::string &zoneName)
      : provider("aws"),
        instance_regex(DEFAULT_INSTANCE_REGEX),
        account_name(accountName),
        log_level(0),
        log_path("ec2_dns_aws_"),
        refresh_interval(60),
        instance_timeout(120),
        vpc_cidr(vpcCidr),
        zone_name(zoneName),
        num_asg_records(4),
        asg_dns_tag("twitter:aws:dns-alias"),
        request_batch_size(200),
        request_timeout_ms(1000),
        connect_timeout_ms(1000)
    { }

    std::string provider;

    std::string aws_access_key;
    std::string aws_secret_key;

    std::string instance_regex;
    std::string account_name;

    int log_level;
    std::string log_path;

    int refresh_interval;
    int instance_timeout;

    std::string vpc_cidr;
    std::string zone_name;

    size_t num_asg_records;
    std::string asg_dns_tag;

    int request_batch_size;
    int request_timeout_ms;
    int connect_timeout_ms;

    std::string region;
    std::string region_code;

    bool TryLoad(const std::string& file);
};

class Instance {
public:
    Instance(const std::string &instanceId,
             const std::string &privateIp,
             const std::string &az)
        : m_instanceId(instanceId), m_privateIp(privateIp), m_az(az)
    {}

    inline const std::string& GetInstanceId() const{ return m_instanceId; }
    inline const std::string& GetPrivateIpAddress() const{ return m_privateIp; }
    inline const std::string& GetZone() const{ return m_az; }

private:
    std::string m_instanceId;
    std::string m_privateIp;
    std::string m_az;
};

class CloudDnsClient {
public:
    CloudDnsClient(
        log_t *logCb,
        const CloudDnsConfig config, std::shared_ptr<StatsReceiver> statsReceiver)
      : m_log(logCb), m_config(config), m_throttler(new RequestThrottler()),
        m_hostCache("host", statsReceiver, config.instance_timeout),
        m_asgCache("asg", statsReceiver, config.instance_timeout),
        m_apiFailures(statsReceiver->Create("api_failure")),
        m_apiRequests(statsReceiver->Create("api_requests")),
        m_apiSuccesses(statsReceiver->Create("api_success")),
        m_lookupRequests(statsReceiver->Create("a_requests")),
        m_reverseLookupRequests(statsReceiver->Create("ptr_requests")),
        m_autoscalerRequests(statsReceiver->Create("autoscaler_requests"))
    {}

    void LaunchRefreshThread() {
      this->m_refreshThread = std::thread(&CloudDnsClient::_RefreshInstanceData, this);
    }

    bool TryResolveIp(const std::string &instanceId, const std::string &clientAddr, std::string *ip);
    bool TryResolveHostname(const std::string &ip, const std::string &clientAddr, std::string *hostname);
    bool TryResolveAutoscaler(const std::string &name, const std::string &clientAddr, std::vector<std::string> *nodes);

protected:
    void _RefreshInstanceData();
    void _RefreshAutoscalerDataImpl(const std::vector<Instance>& instances);
    void _RefreshInstanceDataImpl();
    virtual bool _DescribeInstances(const std::string& instanceId, const std::string& ip, std::vector<Instance> *instances) = 0;
    virtual bool _DescribeAutoscalingGroups(std::unordered_map<std::string, const std::unordered_set<std::string>> *results) = 0;

    log_t *m_log;

    CloudDnsConfig m_config;
    std::unique_ptr<RequestThrottler> m_throttler;

    Cache<std::string> m_hostCache;
    Cache<std::vector<std::string>> m_asgCache;


    std::shared_ptr<Stat> m_cacheHits, m_cacheMisses,
        m_apiFailures, m_apiRequests, m_apiSuccesses,
        m_lookupRequests, m_reverseLookupRequests, m_autoscalerRequests;

private:
    const std::string _GetHostname(const Instance& instance);

    bool _CheckHostCache(const std::string& instanceId, std::string *ip);

    bool _Resolve(const std::string &key, const std::string &clientAddr, const std::function<bool(const std::string&, std::string*)> valueFactory, std::string *value);
    bool _QueryInstanceById(const std::string& instanceId, std::string *ip);
    bool _QueryInstanceByIp(const std::string& ip, std::string *hostname);


    std::thread m_refreshThread;
};

#endif //AWSDNS_EC2DNSCLIENT_H
