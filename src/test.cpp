//
// Created by Steve Niemitz on 1/11/16.
//
#include "Ec2DnsClient.h"

void log(int level, const char *str, ...) {
  std::cout << str << std::endl;
}

int main(int argc, char **argv) {
  auto ec2Client = Aws::MakeShared<Aws::EC2::EC2Client>("test");
  Ec2DnsConfig cfg;

  auto client = new Ec2DnsClient(&log, ec2Client, "test", cfg);

  std::this_thread::sleep_for(std::chrono::seconds(2));


  Aws::String instanceId;
  if (argc > 1) {
    instanceId = argv[1];
  }
  Aws::String ip;
  client->ResolveInstanceIp(instanceId, &ip);
  std::cout << "Got " << ip;

  std::this_thread::sleep_for(std::chrono::seconds(60));
}