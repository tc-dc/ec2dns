//
// Created by Steve Niemitz on 1/11/16.
//
#include "Ec2DnsClient.h"
#include "HostMatcher.h"

void logit(int level, const char *str, ...) {
  std::cout << str << std::endl;
}

isc_result_t putrr(dns_sdlzlookup_t *lookup,
   const char *type,
   dns_ttl_t ttl,
   const char *data) {
  return ISC_R_SUCCESS;
}

int main(int argc, char **argv) {
  auto cfg = Ec2DnsConfig();
  auto hm = HostMatcher(cfg);
  std::string instanceId, awsRegion;
  hm.TryMatch("ue1a-a4532e5d-td", &instanceId, &awsRegion);

  void *data;
  char *argv2[] {(char*)"a", (char*)"b"};
  dlz_create("test", 2, argv2, &data, "log", &logit, "putrr", &putrr, nullptr);

  std::this_thread::sleep_for(std::chrono::seconds(10));
}