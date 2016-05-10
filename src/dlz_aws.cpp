#include <stdarg.h>
#include <math.h>
#include <memory>
#include <random>
#include <unordered_set>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>

#include "dlz_minimal.h"
#include "Ec2DnsClient.h"
#include "HostMatcher.h"
#include "KRandom.h"
#include "ReverseLookupHelper.h"
#include "Stats.h"

#include "aws/core/auth/AWSCredentialsProvider.h"
#include "aws/core/utils/StringUtils.h"
#include "aws/core/utils/logging/AWSLogging.h"
#include "aws/core/utils/logging/DefaultLogSystem.h"

#include <arpa/inet.h>
#include <dlz_minimal.h>

using namespace Aws::EC2;
using namespace Aws::Utils;

struct dlz_state {
    std::shared_ptr<Ec2DnsClient> client;
    std::unique_ptr<HostMatcher> matcher;
    std::unique_ptr<ReverseLookupHelper> rl_helper;
    std::unique_ptr<StatsServer> stats_server;
    std::shared_ptr<StatsReceiver> stats_receiver;
    std::string soa_data;
    std::string zone_name;
    std::string alt_zone_name;
    std::string autoscaler_zone_name;
    size_t num_asg_records;
    DlzCallbacks callbacks;
};

isc_result_t get_src_address(dns_clientinfomethods_t *methods,
    dns_clientinfo_t *clientinfo, std::string *srcAddress) {
  isc_result_t ret;
  if (methods != NULL && methods->version - methods->age >= DNS_CLIENTINFOMETHODS_VERSION) {
    isc_sockaddr_t *addr;
    if ((ret = methods->sourceip(clientinfo, &addr)) != ISC_R_SUCCESS) {
      return ret;
    }
    char buf[100];
    const char* retAddr;
    switch (addr->type.sa.sa_family) {
      case AF_INET:
        retAddr = inet_ntop(AF_INET, &addr->type.sin.sin_addr, buf, sizeof(buf));
        break;
      case AF_INET6:
        retAddr = inet_ntop(AF_INET6, &addr->type.sin6.sin6_addr, buf, sizeof(buf));
        break;
      default:
        return ISC_R_FAILURE;
    }
    if (retAddr == NULL) {
      return ISC_R_FAILURE;
    }
    *srcAddress = buf;
    return ISC_R_SUCCESS;
  }
  return ISC_R_FAILURE;
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
    /* argv is _, zone, vpc_cidr, account_name */
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

  if (argc < 4) {
    cbs.log(ISC_LOG_CRITICAL, "Unable to create ec2dns client, "
        "expected args [zone] [vpc_cidr] [account_name]");
    return ISC_R_FAILURE;
  }
  cbs.log(ISC_LOG_WARNING, "Creating EC2 client");

  Ec2DnsConfig dnsConfig(argv[3], argv[2], argv[1]);
  dnsConfig.TryLoad("/etc/ec2dns.conf");

  Logging::InitializeAWSLogging(
      Aws::MakeShared<Logging::DefaultLogSystem>(
          "log", (Logging::LogLevel)dnsConfig.log_level, dnsConfig.log_path));

  std::shared_ptr<EC2Client> ec2Client;
  std::shared_ptr<AutoScalingClient> asgClient;
  if (!dnsConfig.aws_access_key.empty() && !dnsConfig.aws_secret_key.empty()) {
    Aws::Auth::AWSCredentials creds(
        dnsConfig.aws_access_key,
        dnsConfig.aws_secret_key);
    ec2Client = std::make_shared<EC2Client>(creds, dnsConfig.client_config);
    asgClient = std::make_shared<AutoScalingClient>(creds, dnsConfig.client_config);
  }
  else {
    ec2Client = std::make_shared<EC2Client>(dnsConfig.client_config);
    asgClient = std::make_shared<AutoScalingClient>(dnsConfig.client_config);
  }

  auto state = new dlz_state();
  state->stats_receiver = std::make_shared<StatsReceiver>();
  state->num_asg_records = dnsConfig.num_asg_records;
  state->client = std::make_shared<Ec2DnsClient>(
          cbs.log,
          ec2Client,
          asgClient,
          dnsConfig,
          state->stats_receiver);
  state->zone_name = argv[1];
  if (state->zone_name == "aws.ue1.tcdc.io") {
    state->alt_zone_name = "aws.tcdc.io";
  } else {
    state->alt_zone_name = state->zone_name;
  }
  state->autoscaler_zone_name = "asg." + state->zone_name;
  state->callbacks = cbs;
  state->matcher = std::unique_ptr<HostMatcher>(new HostMatcher(dnsConfig));
  state->rl_helper = std::unique_ptr<ReverseLookupHelper>(new ReverseLookupHelper(state->client));
  if (!state->rl_helper->InitializeReverseLookupZones(argv[2])) {
    printf("ec2dns - Unable to load reverse lookup zones");
    return ISC_R_FAILURE;
  }

  state->client->LaunchRefreshThread();

  Aws::OStringStream soaData;
  soaData << state->zone_name
          << " hostmaster." << state->zone_name
          << " 123 900 600 86400 3600";
  state->soa_data = soaData.str();
  state->stats_server = std::unique_ptr<StatsServer>(new StatsServer(8123, state->stats_receiver));
  state->stats_server->Start();
  *dbdata = state;

  cbs.log(ISC_LOG_WARNING, "EC2 client created");
  return ISC_R_SUCCESS;
}

void dlz_destroy(void *dbdata) {
  delete static_cast<dlz_state *>(dbdata);
  Logging::ShutdownAWSLogging();
}

isc_result_t dlz_findzonedb(void *dbdata, const char *name) {
  auto state = static_cast<dlz_state *>(dbdata);
  // Are we doing a plain old instance lookup?
  if (StringUtils::CaselessCompare(state->zone_name.c_str(), name)) {
    return ISC_R_SUCCESS;
  }
  if (StringUtils::CaselessCompare(state->alt_zone_name.c_str(), name)) {
    return ISC_R_SUCCESS;
  }
  // Are we doing an autoscaler lookup?
  if (StringUtils::CaselessCompare(state->autoscaler_zone_name.c_str(), name)) {
    return ISC_R_SUCCESS;
  }
  // Are we doing a reverse lookup?
  if (state->rl_helper->IsReverseLookupZone(name)) {
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
  if (state->rl_helper->IsReverseLookupZone(zone)) {
    std::string hostName, clientAddr;
    get_src_address(methods, clientinfo, &clientAddr);
    if (state->rl_helper->DoReverseLookup(zone, name, clientAddr, &hostName)) {
      state->callbacks.putrr(lookup, "PTR", 120, hostName.c_str());
      return ISC_R_SUCCESS;
    }
    else {
      return ISC_R_NOTFOUND;
    }
  }

  if (StringUtils::CaselessCompare(state->autoscaler_zone_name.c_str(), zone)) {
    std::string clientAddr;
    std::vector<std::string> nodes;
    get_src_address(methods, clientinfo, &clientAddr);
    if (state->client->TryResolveAutoscaler(name, clientAddr, &nodes)) {
      size_t maxNodes = std::min(nodes.size(), state->num_asg_records);
      for (const auto&& node : k_random<std::string>(nodes, maxNodes)) {
        state->callbacks.putrr(lookup, "A", 120, node.c_str());
      }
      return ISC_R_SUCCESS;
    }
    else {
      return ISC_R_NOTFOUND;
    }
  }

  std::string instanceId, awsZone;
  bool matched = state->matcher->TryMatch(name, &instanceId, &awsZone);
  if (!matched || instanceId.empty() || awsZone.empty()) {
    return ISC_R_NOTFOUND;
  }

  std::string ip, clientAddr;
  get_src_address(methods, clientinfo, &clientAddr);
  auto success = state->client->TryResolveIp(instanceId, clientAddr, &ip);
  if (success) {
    return state->callbacks.putrr(lookup, "A", 120, ip.c_str());
  }
  return ISC_R_NOTFOUND;
}

}