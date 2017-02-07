#ifndef AWSDNS_EC2DNSCLIENT_H
#define AWSDNS_EC2DNSCLIENT_H

#include "dlz_minimal.h"
#include "Cache.h"
#include "Instance.h"
#include "Stats.h"
#include "RequestThrottler.h"
#include "CloudDnsConfig.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <aws/core/client/ClientConfiguration.h>
#include <boost/regex.hpp>
#include <boost/thread/mutex.hpp>
#include <glog/logging.h>

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
    std::shared_ptr<StatsReceiver> stats_receiver;
    std::string soa_data;
    std::string zone_name;
    std::string autoscaler_zone_name;
    size_t num_asg_records;
    DlzCallbacks callbacks;
};


class CloudDnsClient {
public:
    CloudDnsClient(
        const CloudDnsConfig config, std::shared_ptr<StatsReceiver> statsReceiver)
      : m_config(config), m_throttler(new RequestThrottler(statsReceiver)),
        m_hostCache("host", statsReceiver, config.instance_timeout),
        m_asgCache("asg", statsReceiver, config.instance_timeout),
        m_apiFailures(statsReceiver->Create("api_failure")),
        m_apiRequests(statsReceiver->Create("api_requests")),
        m_apiSuccesses(statsReceiver->Create("api_success")),
        m_lookupRequests(statsReceiver->Create("a_requests")),
        m_reverseLookupRequests(statsReceiver->Create("ptr_requests")),
        m_autoscalerRequests(statsReceiver->Create("autoscaler_requests")),
        m_pending(0)
    {}

    virtual ~CloudDnsClient();

    void LaunchRefreshThread();

    bool TryResolveIp(const std::string &instanceId, const std::string &clientAddr, std::string *ip);
    bool TryResolveHostname(const std::string &ip, const std::string &clientAddr, std::string *hostname);
    bool TryResolveAutoscaler(const std::string &name, const std::string &clientAddr, std::vector<std::string> *nodes);

protected:
    void _RefreshInstanceData();
    void _RefreshInstanceDataImpl(bool force = false);
    virtual void _AfterRefresh() {}
    bool _CheckHostCache(const std::string& instanceId, std::string *ip);

    virtual void _RefreshAutoscalerDataImpl(const std::vector<std::unique_ptr<Instance>>& instances);
    virtual bool _DescribeInstances(const std::string& instanceId, const std::string& ip, std::vector<std::unique_ptr<Instance>> *instances) = 0;
    virtual bool _DescribeAutoscalingGroups(std::unordered_map<std::string, const std::unordered_set<std::string>> *results) = 0;

    virtual bool _QueryInstanceById(const std::string& instanceId, const std::string& clientAddr, std::string *ip);
    virtual bool _QueryInstanceByIp(const std::string& ip, const std::string& clientAddr, std::string *hostname);

    CloudDnsConfig m_config;
    std::unique_ptr<RequestThrottler> m_throttler;
    Cache<std::string> m_hostCache;
    Cache<std::vector<std::string>> m_asgCache;
    std::shared_ptr<Stat> m_cacheHits, m_cacheMisses,
        m_apiFailures, m_apiRequests, m_apiSuccesses,
        m_lookupRequests, m_reverseLookupRequests, m_autoscalerRequests;

private:
    const std::string _GetHostname(const std::unique_ptr<Instance>& instance);
    bool _Resolve(const std::string &key, const std::string &clientAddr, const std::function<bool(const std::string&, const std::string&, std::string*)> valueFactory, std::string *value);

    class Pending {
     public:
      Pending(std::atomic_uint_fast32_t *val) : m_val(val) { (*m_val)++; }
      ~Pending() { (*m_val)--; }

      Pending(Pending&&) = delete;
      Pending(const Pending&) = delete;
     private:
      std::atomic_uint_fast32_t* m_val;
    };

    std::atomic_uint_fast32_t m_pending;
    boost::timed_mutex m_shutdownLock;
    std::thread m_refreshThread;
};

#endif //AWSDNS_EC2DNSCLIENT_H
