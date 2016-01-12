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

    Aws::String instance_regex;

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

  bool ResolveInstanceIp(const Aws::String& instanceId, Aws::String *ip);

private:
  class CacheEntry {
  public:
      CacheEntry() { }

      CacheEntry(const Aws::String &ip, const time_point<steady_clock> expiresOn):
        m_ip(ip), m_expiresOn(expiresOn) { }

      Aws::String GetIp() {
        return m_ip;
      }

      bool IsValid() {
        return steady_clock::now() < m_expiresOn;
      }
  private:
      Aws::String m_ip;
      time_point<steady_clock> m_expiresOn;
  };

  void _RefreshInstanceData();
  void _RefreshInstanceDataImpl();
  bool _CheckCache(const std::string& instanceId, std::string *ip);
  void _InsertCache(const std::string& instanceId, const std::string& ip);
  void _InsertCache(const std::string& instanceId, const std::string& ip, const time_point<steady_clock> expiresOn);
  bool _QueryInstance(const std::string& instanceId, std::string *ip);
  bool _DescribeInstances(const std::string& instanceId, Aws::Vector<Model::Instance> *instances);

  Ec2DnsConfig m_config;
  log_t *m_log;
  std::string m_zoneName;
  std::shared_ptr<EC2Client> m_ec2Client;

  std::unordered_map<Aws::String, CacheEntry> m_cache;
  std::mutex m_cacheLock;
  std::thread m_refreshThread;
  CacheStats m_stats;
};


#endif //AWSDNS_EC2DNSCLIENT_H
