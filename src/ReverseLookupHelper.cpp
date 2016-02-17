#include <arpa/inet.h>
#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>

#include "ReverseLookupHelper.h"

bool ReverseLookupHelper::InitializeReverseLookupZones(const std::string &vpcCidr) {
  std::vector<std::string> split;
  boost::algorithm::split(split, vpcCidr, boost::is_any_of("/"));
  if (split.size() != 2) {
    return false;
  }

  auto ip = split[0];
  auto bitsStr = split[1];

  uint32_t ipBits;
  inet_pton(AF_INET, ip.c_str(), &ipBits);
  ipBits &= 0x00FFFFFF;

  uint8_t bits = (uint8_t)atoi(bitsStr.c_str());
  if (bits < 8 || bits > 24) {
    // The mask is too large or small.
    return false;
  }
  uint32_t maskRange = (1 << ((32 - bits) - 8));

  //Generate all class C subnets that fall inside the CIDR.
  for (int a = 0; a < maskRange; a++) {
    char buff[100] = {0};
    uint32_t flipped = htonl(ipBits);
    inet_ntop(AF_INET, &flipped, buff, sizeof(buff));
    std::string cidrStr(buff + 2);
    this->m_rlZones.insert(cidrStr + ".in-addr.arpa");
    ipBits += (1 << 16);
  }

  return true;
}

bool ReverseLookupHelper::IsReverseLookupZone(const std::string& zone) {
  return (this->m_rlZones.find(zone) != this->m_rlZones.end());
}

std::string _ReverseOctets(const std::string &ip) {
  uint32_t inetAddr;
  inet_pton(AF_INET, ip.c_str(), &inetAddr);
  inetAddr = ntohl(inetAddr);
  char buffer[100];
  inet_ntop(AF_INET, &inetAddr, buffer, sizeof(buffer));
  return std::string(buffer);
}

bool ReverseLookupHelper::DoReverseLookup(const std::string& zone, const std::string& name, const std::string &clientAddr, std::string *hostname) {
  std::string fullIp = name + "." + zone;
  if (fullIp.length() < 14) {
    return false;
  }

  fullIp = fullIp.substr(0, fullIp.length() - 13); //13 = len(".in-addr.arpa")
  fullIp = _ReverseOctets(fullIp);

  return this->m_dnsClient->TryResolveHostname(fullIp, clientAddr, hostname);
}
