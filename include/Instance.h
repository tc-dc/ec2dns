#ifndef EC2DNS_INSTANCE_H
#define EC2DNS_INSTANCE_H

#include <string>

class Instance {
 public:
  Instance(const std::string &instanceId,
           const std::string &privateIp,
           const std::string &az)
      : m_instanceId(instanceId), m_privateIp(privateIp), m_az(az)
  {}

  inline const std::string& GetInstanceId() const { return m_instanceId; }
  inline const std::string& GetPrivateIpAddress() const { return m_privateIp; }
  inline const std::string& GetZone() const { return m_az; }

 private:
  std::string m_instanceId;
  std::string m_privateIp;
  std::string m_az;
};


#endif //EC2DNS_INSTANCE_H
