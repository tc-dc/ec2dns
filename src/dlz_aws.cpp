#include <stdarg.h>
#include <memory>
#include <unordered_set>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>

#include "dlz_minimal.h"
#include "Ec2DnsClient.h"
#include "HostMatcher.h"
#include "aws/core/auth/AWSCredentialsProvider.h"
#include "aws/core/utils/StringUtils.h"
#include "aws/core/utils/logging/AWSLogging.h"
#include "aws/core/utils/logging/DefaultLogSystem.h"

#include <arpa/inet.h>

using namespace Aws::EC2;
using namespace Aws::Utils;

struct dlz_state {
    std::unique_ptr<Ec2DnsClient> client;
    std::unique_ptr<HostMatcher> matcher;
    std::string soa_data;
    std::string zone_name;
    std::unordered_set<std::string> reverse_lookup_zones;
    DlzCallbacks callbacks;
};

std::string _ReverseOctets(const std::string &ip) {
  uint32_t inetAddr;
  inet_pton(AF_INET, ip.c_str(), &inetAddr);
  inetAddr = ntohl(inetAddr);
  char buffer[100];
  inet_ntop(AF_INET, &inetAddr, buffer, sizeof(buffer));
  return std::string(buffer);
}

isc_result_t _DoReverseLookup(const std::string& zone, const std::string& name,
                              dlz_state *state, dns_sdlzlookup_t *lookup) {
  std::string fullIp = name + "." + zone;
  if (fullIp.length() < 14) {
    return ISC_R_NOTFOUND;
  }

  fullIp = fullIp.substr(0, fullIp.length() - 13); //13 = len(".in-addr.arpa")
  fullIp = _ReverseOctets(fullIp);

  Aws::String hostname;
  if (state->client->ResolveHostname(fullIp, &hostname)) {
    state->callbacks.putrr(lookup, "PTR", 120, hostname.c_str());
    return ISC_R_SUCCESS;
  }
  return ISC_R_NOTFOUND;
}

bool _LoadReverseLookupZones(const std::string &vpcCidr, std::unordered_set<std::string> &zones) {
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
    zones.insert(cidrStr + ".in-addr.arpa");
    ipBits += (1 << 16);
  }

  return true;
}

extern "C" {

int dlz_version(unsigned int *flags) {
  *flags |= DNS_SDLZFLAG_THREADSAFE;
  return DLZ_DLOPEN_VERSION;
}

static void
b9_add_helper(
    DlzCallbacks *cbs,
    const char *helper_name, void *ptr) {
  if (strcmp(helper_name, "log") == 0)
    cbs->log = (log_t *) ptr;
  if (strcmp(helper_name, "putrr") == 0)
    cbs->putrr = (dns_sdlz_putrr_t *) ptr;
  if (strcmp(helper_name, "putnamedrr") == 0)
    cbs->putnamedrr = (dns_sdlz_putnamedrr_t *) ptr;
}

isc_result_t dlz_create(
    const char *dlzname, unsigned int argc, char *argv[],
    void **dbdata, ...) {

  va_list ap;
  va_start(ap, dbdata);
  const char *helper_name;
  DlzCallbacks cbs;

  while ((helper_name = va_arg(ap, const char *)) != NULL) {
    b9_add_helper(&cbs, helper_name, va_arg(ap, void*));
  }
  va_end(ap);

  cbs.log(ISC_LOG_WARNING, "Creating EC2 client");

  Ec2DnsConfig dnsConfig;
  TryLoadEc2DnsConfig("/etc/ec2dns.conf", &dnsConfig);

  Logging::InitializeAWSLogging(
      Aws::MakeShared<Logging::DefaultLogSystem>(
          "log", (Logging::LogLevel)dnsConfig.log_level, dnsConfig.log_path));

  std::shared_ptr<EC2Client> ec2Client;
  if (!dnsConfig.aws_access_key.empty() && !dnsConfig.aws_secret_key.empty()) {
    Aws::Auth::AWSCredentials creds(
        dnsConfig.aws_access_key,
        dnsConfig.aws_secret_key);
    ec2Client = std::make_shared<EC2Client>(creds, dnsConfig.client_config);
  }
  else {
    ec2Client = std::make_shared<EC2Client>(dnsConfig.client_config);
  }

  auto state = new dlz_state();
  state->client = std::unique_ptr<Ec2DnsClient>(
      new Ec2DnsClient(
          cbs.log,
          ec2Client,
          std::string(argv[1]),
          dnsConfig));
  state->zone_name = argv[1];
  state->callbacks = cbs;
  state->matcher = std::unique_ptr<HostMatcher>(new HostMatcher(dnsConfig));
  if (!_LoadReverseLookupZones(std::string(argv[2]), state->reverse_lookup_zones)) {
    printf("ec2dns - Unable to load reverse lookup zones");
    return ISC_R_FAILURE;
  }

  Aws::OStringStream soaData;
  soaData << state->zone_name
          << " hostmaster." << state->zone_name
          << " 123 900 600 86400 3600";
  state->soa_data = soaData.str();

  *dbdata = state;

  cbs.log(ISC_LOG_WARNING, "EC2 client created");
  return ISC_R_SUCCESS;
}

void dlz_destroy(void *dbdata) {
  delete static_cast<dlz_state *>(dbdata);
  Logging::ShutdownAWSLogging();
}

bool _IsReverseLookup(const std::string& zone, const dlz_state *state) {
  return (state->reverse_lookup_zones.find(zone) != state->reverse_lookup_zones.end());
}

isc_result_t dlz_findzonedb(void *dbdata, const char *name) {
  auto state = static_cast<dlz_state *>(dbdata);
  if (StringUtils::CaselessCompare(state->zone_name.c_str(), name)) {
    return ISC_R_SUCCESS;
  }
  // Are we doing a reverse lookup?
  if (_IsReverseLookup(name, state)) {
    return ISC_R_SUCCESS;
  }
  // Nothing we can do
  return ISC_R_NOTFOUND;
}

isc_result_t dlz_lookup(
    const char *zone, const char *name, void *dbdata,
    dns_sdlzlookup_t *lookup, dns_clientinfomethods_t *methods,
    dns_clientinfo_t *clientinfo) {
  auto state = static_cast<dlz_state *>(dbdata);

  if (strcmp(name, "@") == 0) {
    return ISC_R_NOTFOUND;
  }
  if (strcmp(name, "*") == 0) {
    return ISC_R_NOTFOUND;
  }
  if (_IsReverseLookup(zone, state)) {
    return _DoReverseLookup(zone, name, state, lookup);
  }

  std::string instanceId, awsZone;
  bool matched = state->matcher->TryMatch(name, &instanceId, &awsZone);
  if (!matched || instanceId.empty() || awsZone.empty()) {
    state->callbacks.log(ISC_LOG_ERROR, "Invalid format for name %s", name);
    return ISC_R_NOTFOUND;
  }

  Aws::String ip;
  auto success = state->client->ResolveIp(instanceId, &ip);
  if (success) {
    return state->callbacks.putrr(lookup, "A", 120, ip.c_str());
  }
  return ISC_R_NOTFOUND;
}

}