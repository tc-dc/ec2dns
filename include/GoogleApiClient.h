#ifndef EC2DNS_GCEAPICLIENT_H
#define EC2DNS_GCEAPICLIENT_H

#include <chrono>
#include <string>
#include <istream>
#include <ostream>
#include <fstream>
#include <unordered_map>

#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/client/AWSError.h>
#include <aws/core/client/CoreErrors.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/Outcome.h>

#include "CloudDnsClient.h"

using namespace Aws::Client;
using namespace Aws::Http;
using namespace Aws::Utils::Json;

typedef Aws::Utils::Outcome<std::shared_ptr<Aws::Http::HttpResponse>, AWSError<CoreErrors>> HttpResponseOutcome;
typedef Aws::Utils::Outcome<std::shared_ptr<JsonValue>, AWSError<Aws::Client::CoreErrors>> JsonResponseOutcome;

typedef std::unordered_map<std::string, std::string> Headers;

class Credentials {
public:
    virtual std::string GetClientId() = 0;
    virtual std::string GetClientSecret() = 0;
    virtual std::string GetRefreshToken() = 0;
};


class InvalidCredentials : public Credentials {

};


class StaticOauth2Credentials : public Credentials {
 public:
  StaticOauth2Credentials(const std::string &clientId, const std::string &clientSecret,
                          const std::string &refreshToken)
      : m_clientId(clientId), m_clientSecret(clientSecret), m_refreshToken(refreshToken) {}

  virtual std::string GetClientId() { return m_clientId; }

  virtual std::string GetClientSecret() { return m_clientSecret; }

  virtual std::string GetRefreshToken() { return m_refreshToken; }

 private:
  std::string m_clientId, m_clientSecret, m_refreshToken;
};


class Token {
 public:
  Token() {}
  Token(const std::string& token, int expires_in)
      : m_token(std::string("Bearer ") + token),
        m_expiresOn(std::chrono::system_clock::now() + std::chrono::seconds(expires_in - 10))
  {}

  bool IsValid() { return !m_token.empty() && std::chrono::system_clock::now() < m_expiresOn; }
  const std::string GetToken() { return m_token; }

 private:
  std::string m_token;
  std::chrono::system_clock::time_point m_expiresOn;
};


class GoogleApiClient {
 public:
  static bool TryCreate(const CloudDnsConfig& dnsConfig, const ClientConfiguration &clientConfig, GoogleApiClient** client) {
    std::unique_ptr<Credentials> credentials;
    if (dnsConfig.credentials_file.empty()) {
      return false;
    }
    else {
      std::ifstream f(dnsConfig.credentials_file);
      if (f.fail()) {
        // File didnt exist
        return false;
      }

      Aws::Utils::Json::JsonValue root(f);
      if (!root.WasParseSuccessful()) {
        // Not valid json
        return false;
      }

      credentials = std::unique_ptr<StaticOauth2Credentials>(new StaticOauth2Credentials(
          root.GetString("client_id"),
          root.GetString("client_secret"),
          root.GetString("refresh_token")
      ));
    }
    *client = new GoogleApiClient(dnsConfig, clientConfig, std::move(credentials));
    return true;
  }

 protected:
  GoogleApiClient(const CloudDnsConfig& dnsConfig, const ClientConfiguration &clientConfig, std::unique_ptr<Credentials> credentials)
      : m_clientConfig(clientConfig),
        m_httpClient(Aws::Http::CreateHttpClient(clientConfig)),
        m_credentials(std::move(credentials))
  {
  }

 public:
  HttpResponseOutcome MakeRequest(const std::string &uri, Aws::Http::HttpMethod method, const Headers& headers, std::shared_ptr<std::iostream> body);
  JsonResponseOutcome MakeJsonRequest(const std::string &uri, Aws::Http::HttpMethod method, const Headers& headers, const JsonValue &body);
  JsonResponseOutcome CallApi(const std::string &uri, Aws::Http::HttpMethod method, const JsonValue &body);

 protected:
  AWSError<CoreErrors> _BuildError(const std::shared_ptr<HttpResponse> &response) const;
  void _RefreshToken();
  std::shared_ptr<std::iostream> _UrlEncode(const std::unordered_map<std::string, std::string> &values);
  JsonResponseOutcome _JsonDecodeResponse(const HttpResponseOutcome &response);

 private:
  Aws::Client::ClientConfiguration m_clientConfig;
  std::shared_ptr<Aws::Http::HttpClient> m_httpClient;
  std::unique_ptr<Credentials> m_credentials;
  Token m_token;
};

#endif //EC2DNS_GCEAPICLIENT_H
