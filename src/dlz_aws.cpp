#include <stdarg.h>
#include <math.h>
#include <memory>
#include <random>
#include <unordered_set>

#include <arpa/inet.h>
#include <dlz_minimal.h>

#include "BindLogSink.h"
#include "CloudDnsClient.h"
#include "HostMatcher.h"
#include "KRandom.h"
#include "ReverseLookupHelper.h"
#include "Stats.h"

#include "aws/core/utils/StringUtils.h"

#ifdef WITH_GCE
#include "gce/GceDnsClient.h"
#endif

#ifdef WITH_AWS
#include "ec2/Ec2DnsClient.h"
#endif



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

  CloudDnsConfig dnsConfig(argv[3], argv[2], argv[1]);
  dnsConfig.TryLoad("/etc/ec2dns.conf");

  google::InitGoogleLogging("clouddns");
  google::AddLogSink(new BindLogSink(cbs.log));

  auto state = new dlz_state();
  state->stats_receiver = std::make_shared<StatsReceiver>();
  state->num_asg_records = dnsConfig.num_asg_records;
  state->zone_name = argv[1];
  state->autoscaler_zone_name = "asg." + state->zone_name;
  state->callbacks = cbs;
  state->matcher = std::unique_ptr<HostMatcher>(new HostMatcher(dnsConfig));

  if (dnsConfig.provider == "aws") {
#ifdef WITH_AWS
    state->client = Ec2DnsClient::Create(dnsConfig, state->stats_receiver);
#else
    cbs.log(ISC_LOG_CRITICAL, "Provider was AWS but we weren't built with AWS support :'(");
    return ISC_R_FAILURE;
#endif
  } else if (dnsConfig.provider == "gce") {
#if WITH_GCE
    state->client = GceDnsClient::Create(dnsConfig, state->stats_receiver);
#else
    cbs.log(ISC_LOG_CRITICAL, "Provider was GCE but we weren't built with GCE support :'(");
    return ISC_R_FAILURE;
    #endif
  } else {
    cbs.log(ISC_LOG_CRITICAL, "Unknown provider set");
    return ISC_R_FAILURE;
  }

  state->rl_helper = std::unique_ptr<ReverseLookupHelper>(new ReverseLookupHelper(state->client));
  if (!state->rl_helper->InitializeReverseLookupZones(argv[2])) {
    printf("ec2dns - Unable to load reverse lookup zones");
    return ISC_R_FAILURE;
  }

  state->client->LaunchRefreshThread();

  std::ostringstream soaData;
  soaData << state->zone_name
          << " hostmaster." << state->zone_name
          << " 123 172800 900 1209600 180";
  state->soa_data = soaData.str();
  state->stats_server = std::unique_ptr<StatsServer>(new StatsServer(8123, state->stats_receiver));
  state->stats_server->Start();
  *dbdata = state;

  cbs.log(ISC_LOG_WARNING, "EC2 client created");
  return ISC_R_SUCCESS;
}

void dlz_destroy(void *dbdata) {
  delete static_cast<dlz_state *>(dbdata);
}

isc_result_t dlz_findzonedb(void *dbdata, const char *name) {
  auto state = static_cast<dlz_state *>(dbdata);
  // Are we doing a plain old instance lookup?
  if (Aws::Utils::StringUtils::CaselessCompare(state->zone_name.c_str(), name)) {
    return ISC_R_SUCCESS;
  }
  // Are we doing an autoscaler lookup?
  if (Aws::Utils::StringUtils::CaselessCompare(state->autoscaler_zone_name.c_str(), name)) {
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
    state->callbacks.putrr(lookup, "SOA", 120, state->soa_data.c_str());
    return ISC_R_SUCCESS;
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

  if (strlen(name) > 3 && strncmp(name, "ip-", 3) == 0) {
    std::string sname(name);
    std::replace(sname.begin(), sname.end(), '-', '.');
    sname = sname.substr(3);
    state->callbacks.putrr(lookup, "A", 120, sname.c_str());
    return ISC_R_SUCCESS;
  }

  if (Aws::Utils::StringUtils::CaselessCompare(state->autoscaler_zone_name.c_str(), zone)) {
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
  } else {
    return ISC_R_NOTFOUND;
  }
  return ISC_R_NOTFOUND;
}

}