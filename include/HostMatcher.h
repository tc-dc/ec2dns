//
// Created by Steve Niemitz on 1/12/16.
//

#ifndef EC2DNS_HOSTMATCHER_H
#define EC2DNS_HOSTMATCHER_H

#include "Ec2DnsClient.h"
#include <boost/regex.hpp>

class HostMatcher {
public:
    HostMatcher(const Ec2DnsConfig &config)
      : m_hostRegex(config.instance_regex)
    {  }

    bool TryMatch(const std::string &host,
        std::string *instanceId,
        std::string *awsZone) {

      boost::smatch matches;
      if (!boost::regex_match(host, matches, m_hostRegex)) {
        return false;
      }
      *instanceId = "i-" + static_cast<std::string>(matches["instanceId"]);
      *awsZone = static_cast<std::string>(matches["region"]);
      return true;
    }

private:
    boost::regex m_hostRegex;
};

#endif //EC2DNS_HOSTMATCHER_H
