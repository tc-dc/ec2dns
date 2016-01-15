#pragma once

#include <string>
#include <unordered_set>

#include "Ec2DnsClient.h"

class ReverseLookupHelper {
public:
  ReverseLookupHelper(std::shared_ptr<Ec2DnsClient> dnsClient)
    : m_dnsClient(dnsClient) { }

  bool InitializeReverseLookupZones(const std::string& vpcCidr);
  bool IsReverseLookupZone(const std::string& zone);
  bool DoReverseLookup(const std::string& zone, const std::string& name, std::string *hostname);

private:
  std::shared_ptr<Ec2DnsClient> m_dnsClient;
  std::unordered_set<std::string> m_rlZones;
};