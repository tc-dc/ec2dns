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
#include "aws/autoscaling/AutoScalingClient.h"
#include "aws/autoscaling/model/DescribeAutoScalingGroupsRequest.h"
#include "aws/ec2/EC2Client.h"
#include "aws/ec2/model/DescribeInstancesRequest.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/regex.hpp>

using namespace Aws::AutoScaling;
using namespace Aws::EC2;
using namespace std::chrono;

struct DlzCallbacks {
  log_t *log;
  dns_sdlz_putrr_t *putrr;
  dns_sdlz_putnamedrr_t *putnamedrr;
};

#define DEFAULT_INSTANCE_REGEX "^(?<region>[a-z]{2}\\d)(?<zone>[a-z])-(?<account>\\w+)-(?<instanceId>\\w*)$"

class Ec2DnsConfig {
public:
    Ec2DnsConfig(const std::string& accountName, const std::string& vpcCidr, const std::string &zoneName)
      : client_config(),
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
        request_batch_size(200)
    { }

    Aws::String aws_access_key;
    Aws::String aws_secret_key;

    Aws::Client::ClientConfiguration client_config;

    std::string instance_regex;
    std::string account_name;

    int log_level;
    Aws::String log_path;

    int refresh_interval;
    int instance_timeout;

    std::string vpc_cidr;
    std::string zone_name;

    size_t num_asg_records;
    std::string asg_dns_tag;

    int request_batch_size;

    std::string region_code;

    bool TryLoad(const std::string& file);
};

class Ec2DnsClient {
public:
  Ec2DnsClient(
    log_t *logCb,
    const std::shared_ptr<EC2Client> ec2Client,
    const std::shared_ptr<AutoScalingClient> asgClient,
    const Ec2DnsConfig config,
    std::shared_ptr<StatsReceiver> statsReceiver
  )
    : m_hostCache("host", statsReceiver, config.instance_timeout),
      m_asgCache("asg", statsReceiver, config.instance_timeout),
      m_config(config), m_ec2Client(ec2Client), m_asgClient(asgClient),
      m_log(logCb), m_throttler(new RequestThrottler()),
      m_apiFailures(statsReceiver->Create("api_failure")),
      m_apiRequests(statsReceiver->Create("api_requests")),
      m_apiSuccesses(statsReceiver->Create("api_success")),
      m_lookupRequests(statsReceiver->Create("a_requests")),
      m_reverseLookupRequests(statsReceiver->Create("ptr_requests")),
      m_autoscalerRequests(statsReceiver->Create("autoscaler_requests"))
  {
  }

  void LaunchRefreshThread() {
    this->m_refreshThread = std::thread(&Ec2DnsClient::_RefreshInstanceData, this);
  }

  bool TryResolveIp(const std::string &instanceId, const std::string &clientAddr, std::string *ip);
  bool TryResolveHostname(const std::string &ip, const std::string &clientAddr, std::string *hostname);
  bool TryResolveAutoscaler(const std::string &name, const std::string &clientAddr, std::vector<std::string> *nodes);

protected:
  void _RefreshAutoscalerDataImpl(const Aws::Vector<Aws::EC2::Model::Instance>& instances);
  void _RefreshInstanceData();
  void _RefreshInstanceDataImpl();

private:
    template<class TRequest, class TResponse, class TError>
  bool _CallApi(
      std::string apiTag,
      TRequest& request,
      std::function<Aws::Utils::Outcome<TResponse, Aws::Client::AWSError<TError>>(const TRequest&)> requestFn,
      std::vector<TResponse> *responses) {
    std::string nextToken;
    do {
      this->m_apiRequests->Increment();
      auto ret = requestFn(request);
      this->m_log(ISC_LOG_INFO, "ec2dns - API Request complete");
      if (!ret.IsSuccess()) {
        this->m_apiFailures->Increment();
        auto errorMessage = ret.GetError().GetMessage();
        this->m_log(
            ISC_LOG_ERROR,
            "ec2dns - API request %s failed with error: %s",
            apiTag.c_str(),
            errorMessage.c_str());
        return false;
      }
      this->m_apiSuccesses->Increment();

      auto result = ret.GetResult();
      responses->push_back(result);
      nextToken = result.GetNextToken();
      request.SetNextToken(nextToken);
    } while (!nextToken.empty());
    return true;
  };

  const std::string _GetHostname(const Aws::EC2::Model::Instance& instance);

  bool _CheckHostCache(const std::string& instanceId, std::string *ip);
  template<class T>
  bool _CheckCache(
      const std::string &key, T* result,
      const std::unordered_map<std::string, CacheEntry<T>>& cache,
      std::mutex& cacheLock,
      std::shared_ptr<Stat> &hit,
      std::shared_ptr<Stat> &miss
  );
  void _InsertCache(const std::string& instanceId, const std::string& ip);
  void _InsertCache(const std::string& instanceId, const std::string& ip, const time_point<steady_clock> expiresOn);
  void _InsertCacheNoLock(const std::string& instanceId, const std::string& ip, const time_point<steady_clock> expiresOn);

  bool _Resolve(const std::string &key, const std::string &clientAddr, const std::function<bool(const std::string&, std::string*)> valueFactory, std::string *value);
  bool _QueryInstanceById(const std::string& instanceId, std::string *ip);
  bool _QueryInstanceByIp(const std::string& ip, std::string *hostname);

  bool _DescribeInstances(const std::string& instanceId, const std::string& ip, Aws::Vector<Aws::EC2::Model::Instance> *instances);

  Cache<std::string> m_hostCache;
  Cache<std::vector<std::string>> m_asgCache;

  Ec2DnsConfig m_config;
  std::shared_ptr<EC2Client> m_ec2Client;
  std::shared_ptr<AutoScalingClient> m_asgClient;
  log_t *m_log;
  std::thread m_refreshThread;
  std::unique_ptr<RequestThrottler> m_throttler;

  std::shared_ptr<Stat> m_cacheHits, m_cacheMisses,
      m_apiFailures, m_apiRequests, m_apiSuccesses,
      m_lookupRequests, m_reverseLookupRequests, m_autoscalerRequests;
};


#endif //AWSDNS_EC2DNSCLIENT_H
