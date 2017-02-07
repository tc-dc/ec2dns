#ifndef EC2DNS_INSTANCECREDENTIALS_H_H
#define EC2DNS_INSTANCECREDENTIALS_H_H

#include <chrono>

#include <googleapis/base/callback-types.h>
#include <googleapis/client/transport/http_authorization.h>
#include <googleapis/client/transport/http_transport.h>

using namespace googleapis::client;
using namespace googleapis::util;

class InstanceCredentials : public AuthorizationCredential {
 public:
  InstanceCredentials(HttpTransport* transport)
    : m_transport(transport)
  {}

  const std::string type() const { return m_type; }
  virtual Status Refresh();
  virtual Status AuthorizeRequest(HttpRequest* request);

  // Unimplemented
  virtual Status Load(DataReader* serialized_credential);
  virtual DataReader* MakeDataReader() const;
  virtual void RefreshAsync(googleapis::Callback1<Status>* callback);

 private:
  struct _Token {
    _Token(const std::string& _token, const std::chrono::system_clock::time_point& _expiresOn)
        : token(_token), expiresOn(_expiresOn) {}

    std::string token;
    std::chrono::system_clock::time_point expiresOn;
  };

  HttpRequest* MakeRefreshRequest();
  void FinishRefreshAsync(googleapis::Callback1<Status>* callback, HttpRequest* request);
  Status FinishRefresh(HttpRequest* request);

  HttpTransport* m_transport;
  std::unique_ptr<_Token> m_token;
  static const std::string m_type;
};

#endif //EC2DNS_INSTANCECREDENTIALS_H_H
