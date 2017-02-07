#ifndef EC2DNS_CLOUDDNSCONFIG_H
#define EC2DNS_CLOUDDNSCONFIG_H

#include <string>

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
        connect_timeout_ms(1000),
        max_request_pool_size(10),
        max_request_concurrency(100)
  { }

  std::string provider;

  std::string credentials_file;
  std::string aws_access_key;
  std::string aws_secret_key;

  std::string instance_regex;
  std::string account_name;
  std::string profile_name;

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

  uint32_t max_request_pool_size;
  uint32_t max_request_concurrency;

  bool TryLoad(const std::string& file);
};

#endif //EC2DNS_CLOUDDNSCONFIG_H
