#include <stdarg.h>

#include "dlz_minimal.h"
#include "Ec2DnsClient.h"
#include "aws/core/auth/AWSCredentialsProvider.h"
#include "aws/core/utils/StringUtils.h"
#include "aws/core/utils/logging/AWSLogging.h"
#include "aws/core/utils/logging/DefaultLogSystem.h"

extern "C" {

int dlz_version(unsigned int *flags) {
  *flags |= DNS_SDLZFLAG_THREADSAFE;
  return DLZ_DLOPEN_VERSION;
}

static void
b9_add_helper(DlzCallbacks *cbs,
  const char *helper_name, void *ptr)
{
  if (strcmp(helper_name, "log") == 0)
    cbs->log = (log_t *)ptr;
  if (strcmp(helper_name, "putrr") == 0)
    cbs->putrr = (dns_sdlz_putrr_t *)ptr;
  if (strcmp(helper_name, "putnamedrr") == 0)
    cbs->putnamedrr = (dns_sdlz_putnamedrr_t *)ptr;
}

struct dlz_state {
    Ec2DnsClient* client;
    std::string soa_data;
    std::string zone_name;
    DlzCallbacks callbacks;

    ~dlz_state() {
      delete client;
    }
};

isc_result_t dlz_create(const char *dlzname, unsigned int argc, char *argv[],
  void **dbdata, ...) {

  va_list ap;
  va_start(ap, dbdata);
  const char* helper_name;
  DlzCallbacks cbs;

  while ((helper_name = va_arg(ap, const char *)) != NULL) {
    b9_add_helper(&cbs, helper_name, va_arg(ap, void*));
  }
  va_end(ap);

  cbs.log(ISC_LOG_WARNING, "Creating EC2 client");

  Ec2DnsConfig dnsConfig;
  TryLoadEc2DnsConfig("/etc/ec2dns.conf", &dnsConfig);

  Aws::Utils::Logging::InitializeAWSLogging(
    Aws::MakeShared<Aws::Utils::Logging::DefaultLogSystem>(
      "log", (Aws::Utils::Logging::LogLevel)dnsConfig.log_level, dnsConfig.log_path));

  std::shared_ptr<Aws::EC2::EC2Client> ec2Client;
  if(!dnsConfig.aws_access_key.empty() && !dnsConfig.aws_secret_key.empty()) {
    Aws::Auth::AWSCredentials creds(dnsConfig.aws_access_key, dnsConfig.aws_secret_key);
    ec2Client = Aws::MakeShared<Aws::EC2::EC2Client>("client", creds, dnsConfig.client_config);
  }
  else {
    ec2Client = Aws::MakeShared<Aws::EC2::EC2Client>("client", dnsConfig.client_config);
  }

  auto state = new dlz_state();
  state->client = new Ec2DnsClient(cbs, ec2Client, strdup(argv[1]), dnsConfig);
  state->zone_name = strdup(argv[1]);
  state->callbacks = cbs;

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
  delete static_cast<dlz_state*>(dbdata);
  Aws::Utils::Logging::ShutdownAWSLogging();
}

isc_result_t dlz_findzonedb(void *dbdata, const char *name) {
  auto state = static_cast<dlz_state*>(dbdata);
  auto match = Aws::Utils::StringUtils::CaselessCompare(state->zone_name.c_str(), name);
  return match ? ISC_R_SUCCESS : ISC_R_NOTFOUND;
}

isc_result_t dlz_lookup(const char *zone, const char *name, void *dbdata,
  dns_sdlzlookup_t *lookup,
  dns_clientinfomethods_t *methods,
  dns_clientinfo_t *clientinfo) {
  auto state = static_cast<dlz_state*>(dbdata);

  //state->callbacks.log(ISC_LOG_INFO, "looking up %s for zone %s", name, zone);

  if(strcmp(name, "@") == 0) {
    state->callbacks.putrr(lookup, "SOA", 3600, state->soa_data.c_str());
    state->callbacks.putrr(lookup, "NS", 3600, state->zone_name.c_str());
    return ISC_R_SUCCESS;
  }
  if(strcmp(name, "*") == 0) {
    return ISC_R_NOTFOUND;
  }

  auto split = Aws::Utils::StringUtils::Split(name, '-');
  if(split.size() < 3) {
    state->callbacks.log(ISC_LOG_ERROR, "Invalid format for name %s", name);
    return ISC_R_NOTFOUND;
  }

  Aws::String ip;
  auto success = state->client->ResolveInstanceIp("i-" + split[1], &ip);
  if(success) {
    return state->callbacks.putrr(lookup, "A", 120, ip.c_str());
  }
  return ISC_R_NOTFOUND;
}

}