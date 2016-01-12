//
// Created by Steve Niemitz on 1/12/16.
//

#ifndef EC2DNS_HOSTMATCHER_H
#define EC2DNS_HOSTMATCHER_H

#include "Ec2DnsClient.h"

class HostMatcher {
public:
    HostMatcher(const Ec2DnsConfig &config) :
        m_instanceIdx(config.regex_match_idx_instance),
        m_zoneIdx(config.regex_match_idx_zone),
        m_hostRegex(config.instance_regex)
    { }

    bool TryMatch(const std::string &host,
        std::string *instanceId,
        std::string *awsZone) {

      std::smatch matches;
      auto matched = std::regex_match(host, matches, m_hostRegex);
      if (!matched) {
        return false;
      }
      if (matches.size() <= std::max(m_instanceIdx, m_zoneIdx)) {
        return false;
      }
      *instanceId = "i-" + static_cast<std::string>(matches[m_instanceIdx]);
      *awsZone = static_cast<std::string>(matches[m_zoneIdx]);
      return true;
    }

private:
    int m_instanceIdx;
    int m_zoneIdx;
    std::regex m_hostRegex;
};

#endif //EC2DNS_HOSTMATCHER_H
