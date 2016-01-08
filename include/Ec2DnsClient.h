//
// Created by Steve Niemitz on 1/7/16.
//

#ifndef AWSDNS_EC2DNSCLIENT_H
#define AWSDNS_EC2DNSCLIENT_H

#include "dlz_minimal.h"
#include "aws/core/utils/json/JsonSerializer.h"
#include "aws/ec2/EC2Client.h"
#include "aws/ec2/model/DescribeInstancesRequest.h"

#include <mutex>

using namespace Aws::EC2;

struct DlzCallbacks {
  log_t *log;
  dns_sdlz_putrr_t *putrr;
  dns_sdlz_putnamedrr_t *putnamedrr;
};

class Ec2DnsConfig {
public:
    Ec2DnsConfig()
      : client_config(), log_level(0) { }

    Aws::String aws_access_key;
    Aws::String aws_secret_key;

    Aws::Client::ClientConfiguration client_config;

    int log_level;
    Aws::String log_path;
};

bool TryLoadEc2DnsConfig(Aws::String file, Ec2DnsConfig *config);

class Ec2DnsClient {
public:
  Ec2DnsClient(
    DlzCallbacks callbacks,
    std::shared_ptr<EC2Client> ec2Client,
    std::string zoneName,
    Ec2DnsConfig config
  )
    : m_callbacks(callbacks), m_ec2Client(ec2Client), m_zoneName(zoneName), m_config(config)
  { }

  bool ResolveInstanceIp(Aws::String instanceId, Aws::String *ip);

private:
  Ec2DnsConfig m_config;
  DlzCallbacks m_callbacks;
  std::string m_zoneName;
  std::shared_ptr<EC2Client> m_ec2Client;
  std::mutex m_cacheLock;
};


#endif //AWSDNS_EC2DNSCLIENT_H
