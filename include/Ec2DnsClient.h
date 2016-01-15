//
// Created by Steve Niemitz on 1/7/16.
//

#ifndef AWSDNS_EC2DNSCLIENT_H
#define AWSDNS_EC2DNSCLIENT_H

#include "dlz_minimal.h"
#include "aws/core/utils/json/JsonSerializer.h"
#include "aws/ec2/EC2Client.h"
#include "aws/ec2/model/DescribeInstancesRequest.h"

#include <chrono>
#include <mutex>
#include <unordered_map>

#include <boost/regex.hpp>

using namespace Aws::EC2;
using namespace std::chrono;

struct DlzCallbacks {
  log_t *log;
  dns_sdlz_putrr_t *putrr;
  dns_sdlz_putnamedrr_t *putnamedrr;
};

#define DEFAULT_INSTANCE_REGEX "^(?<region>[a-z]{2}\\d)(?<zone>[a-z])-(?<instanceId>\\w*)-(?<account>\\w{2})$"

class Ec2DnsConfig {
public:
    Ec2DnsConfig()
      : client_config(),
        log_level(0),
        refresh_interval(60),
        instance_timeout(120),
        instance_regex(DEFAULT_INSTANCE_REGEX)
    { }

    Aws::String aws_access_key;
    Aws::String aws_secret_key;

    Aws::Client::ClientConfiguration client_config;

    std::string instance_regex;

    int log_level;
    Aws::String log_path;

    int refresh_interval;
    int instance_timeout;
};

class CacheStats {
public:
    CacheStats() :
      m_hits(0), m_misses(0)
    { }

    inline void Hit() {
      m_hits++;
    }

    inline void Miss() {
      m_misses++;
    }

private:
    std::atomic_uint_fast64_t m_hits;
    std::atomic_uint_fast64_t m_misses;
};

bool TryLoadEc2DnsConfig(Aws::String file, Ec2DnsConfig *config);

class Ec2DnsClient {
public:
  Ec2DnsClient(
    log_t *logCb,
    const std::shared_ptr<EC2Client> ec2Client,
    const std::string zoneName,
    const Ec2DnsConfig config
  )
    : m_log(logCb), m_ec2Client(ec2Client), m_zoneName(zoneName), m_config(config)
  {
    this->m_refreshThread = std::thread(&Ec2DnsClient::_RefreshInstanceData, this);
  }

  bool ResolveIp(const Aws::String& instanceId, Aws::String *ip);
  bool ResolveHostname(const Aws::String& ip, Aws::String *hostname);

private:
  template<class T>
  class CacheEntry {
  public:
      CacheEntry() { }

      CacheEntry(const T &item, const time_point<steady_clock> expiresOn):
        m_item(item), m_expiresOn(expiresOn) { }

      T GetItem() {
        return m_item;
      }

      bool IsValid() {
        return IsValid(steady_clock::now());
      }

      bool IsValid(const time_point<steady_clock> now) {
        return now < m_expiresOn;
      }
  private:
      T m_item;
      time_point<steady_clock> m_expiresOn;
  };

  void _RefreshInstanceData();
  void _RefreshInstanceDataImpl();

  std::string _GetRegionCode(Aws::Region region) {
    switch (region) {
      case Aws::Region::US_EAST_1: return "ue1";
      case Aws::Region::US_WEST_1: return "uw1";
      case Aws::Region::US_WEST_2: return "uw2";
      case Aws::Region::AP_NORTHEAST_1: return "an1";
      case Aws::Region::AP_NORTHEAST_2: return "an2";
      case Aws::Region::AP_SOUTHEAST_1: return "as1";
      case Aws::Region::AP_SOUTHEAST_2: return "as2";
      case Aws::Region::EU_CENTRAL_1: return "ec1";
      case Aws::Region::EU_WEST_1: return "ew1";
      case Aws::Region::SA_EAST_1: return "se1";
    }
  }
  std::string _GetHostname(const Model::Instance& instance);

  bool _CheckCache(const std::string& instanceId, std::string *ip);
  void _InsertCache(const std::string& instanceId, const std::string& ip);
  void _InsertCache(const std::string& instanceId, const std::string& ip, const time_point<steady_clock> expiresOn);
  void _InsertCacheNoLock(const std::string& instanceId, const std::string& ip, const time_point<steady_clock> expiresOn);

  bool _Resolve(const std::string &key, const std::function<bool(const std::string&, std::string*)> valueFactory, std::string *value);
  bool _QueryInstanceById(const std::string& instanceId, std::string *ip);
  bool _QueryInstanceByIp(const std::string& ip, std::string *hostname);

  bool _DescribeInstances(const std::string& instanceId, const std::string& ip, Aws::Vector<Model::Instance> *instances);

  Ec2DnsConfig m_config;
  log_t *m_log;
  std::string m_zoneName;
  std::shared_ptr<EC2Client> m_ec2Client;

  std::unordered_map<Aws::String, CacheEntry<Aws::String>> m_cache;
  std::mutex m_cacheLock;
  std::thread m_refreshThread;
  CacheStats m_stats;
};


#endif //AWSDNS_EC2DNSCLIENT_H
