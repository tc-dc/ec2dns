#include <stdarg.h>

#include "dlz_minimal.h"
#include "Ec2DnsClient.h"
#include "aws/core/auth/AWSCredentialsProvider.h"
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
  TryLoadEc2DnsConfig("/etc/awsdns.conf", &dnsConfig);
  if(dnsConfig.log_path.empty()) {
    dnsConfig.log_path = "aws_api_";
  }

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

  Ec2DnsClient *cli = new Ec2DnsClient(cbs, ec2Client, strdup(argv[1]), dnsConfig);
  *dbdata = cli;

  cbs.log(ISC_LOG_WARNING, "EC2 client created");
  return ISC_R_SUCCESS;
}

void dlz_destroy(void *dbdata) {
  delete static_cast<Ec2DnsClient*>(dbdata);
  Aws::Utils::Logging::ShutdownAWSLogging();
}

isc_result_t dlz_findzonedb(void *dbdata, const char *name) {
  Ec2DnsClient* state = static_cast<Ec2DnsClient*>(dbdata);
  return state->IsAwsZone(name) ? ISC_R_SUCCESS : ISC_R_NOTFOUND;
}

isc_result_t dlz_lookup(const char *zone, const char *name, void *dbdata,
  dns_sdlzlookup_t *lookup,
  dns_clientinfomethods_t *methods,
  dns_clientinfo_t *clientinfo) {

  Ec2DnsClient* client = static_cast<Ec2DnsClient*>(dbdata);
  auto name_str = std::string(name);
  if (client->IsAwsZone(zone) &&
      name_str.compare(0, 2, "i-") == 0) {
    Aws::String ip;
    auto success = client->ResolveInstanceIp(name_str, &ip);
    if(success) {
      return client->Callbacks().putrr(lookup, "A", 120, ip.c_str());
    }
    return ISC_R_NOTFOUND;
  }

  return ISC_R_NOTFOUND;
}

}