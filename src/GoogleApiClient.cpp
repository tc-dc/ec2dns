#include <boost/algorithm/string/join.hpp>
#include <aws/core/utils/stream/ResponseStream.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <aws/core/utils/StringUtils.h>
#include "GoogleApiClient.h"

using namespace Aws::Http;
using namespace Aws::Client;
using namespace Aws::Utils;

static const int SUCCESS_RESPONSE_MIN = 200;
static const int SUCCESS_RESPONSE_MAX = 299;

bool DoesResponseGenerateError(const std::shared_ptr<HttpResponse>& response) {
  if (!response) return true;

  int responseCode = static_cast<int>(response->GetResponseCode());
  return response == nullptr || responseCode < SUCCESS_RESPONSE_MIN || responseCode > SUCCESS_RESPONSE_MAX;
}

static const std::string GOOGLE_OAUTH2_TOKEN_ENDPOINT("https://accounts.google.com/o/oauth2/token");
static const std::string URLENCODED_CONTENT_TYPE("application/x-www-form-urlencoded");


std::shared_ptr<std::iostream> GoogleApiClient::_UrlEncode(const std::unordered_map<std::string, std::string> &values) {
  std::vector<std::string> elements;
  for (auto& kv : values) {
    auto s = StringUtils::URLEncode(kv.first.c_str()) + "=" + StringUtils::URLEncode(kv.second.c_str());
    elements.emplace_back(s);
  }

  auto ptr = std::make_shared<std::stringstream>(boost::algorithm::join(elements, "&"));
  ptr->seekg(0, ptr->end);
  return ptr;
}

void GoogleApiClient::_RefreshToken() {
  std::unordered_map<std::string, std::string> params {
      { std::string("client_id"), this->m_credentials->GetClientId() },
      { std::string("client_secret"), this->m_credentials->GetClientSecret() },
      { std::string("refresh_token"), this->m_credentials->GetRefreshToken() },
      { std::string("grant_type"), "refresh_token"}
  };
  auto requestBody = this->_UrlEncode(params);
  auto headers = Headers {
      {"Content-Type", URLENCODED_CONTENT_TYPE }
  };
  auto httpResp = this->MakeRequest(GOOGLE_OAUTH2_TOKEN_ENDPOINT, HttpMethod::HTTP_POST, headers, requestBody);
  auto jsonResp = _JsonDecodeResponse(httpResp);
  auto& jsValue = jsonResp.GetResult();
  auto token = jsValue->GetString("access_token");
  auto expiresIn = jsValue->GetInteger("expires_in");

  m_token = Token(token, expiresIn);
}

HttpResponseOutcome GoogleApiClient::MakeRequest(
    const std::string& uri,
    Aws::Http::HttpMethod method,
    const std::unordered_map<std::string, std::string>& headers,
    std::shared_ptr<std::iostream> body) {
  std::shared_ptr<HttpRequest> httpRequest(
      CreateHttpRequest(uri, method, Aws::Utils::Stream::DefaultResponseStreamFactoryMethod));
  for (auto& kv : headers) {
    httpRequest->SetHeaderValue(kv.first, kv.second);
  }
  if (body->tellg() > 0) {
    body->seekg(0, body->end);
    auto streamSize = body->tellg();
    body->seekg(0, body->beg);
    Aws::StringStream contentLength;
    contentLength << streamSize;
    httpRequest->SetContentLength(contentLength.str());
    httpRequest->AddContentBody(body);
  }

  std::shared_ptr<HttpResponse> httpResponse(
      this->m_httpClient->MakeRequest(*httpRequest));

  if (DoesResponseGenerateError(httpResponse)) {
    return HttpResponseOutcome(_BuildError(httpResponse));
  }

  return HttpResponseOutcome(httpResponse);
}

JsonResponseOutcome GoogleApiClient::_JsonDecodeResponse(const HttpResponseOutcome &outcome) {
  if (outcome.IsSuccess()) {
    auto &respBody = outcome.GetResult()->GetResponseBody();
    auto jsonVal = std::make_shared<JsonValue>(respBody);
    if (jsonVal->WasParseSuccessful()) {
      return JsonResponseOutcome(jsonVal);
    } else {
      return JsonResponseOutcome(AWSError<CoreErrors>(CoreErrors::INTERNAL_FAILURE, false));
    }
  } else {
    return JsonResponseOutcome(outcome.GetError());
  }
}

JsonResponseOutcome GoogleApiClient::MakeJsonRequest(
    const std::string &uri,
    Aws::Http::HttpMethod method,
    const Headers& headers,
    const JsonValue &body) {
  auto bodyData = std::make_shared<std::stringstream>();
  auto newHeaders = Headers(headers);
  if (body.IsObject()) {
    body.WriteCompact(*bodyData, true);
    newHeaders["Content-Type"] = "application/json";
  }

  auto outcome = MakeRequest(uri, method, newHeaders, bodyData);
  return _JsonDecodeResponse(outcome);
}

JsonResponseOutcome GoogleApiClient::CallApi(const std::string& uri, HttpMethod method, const JsonValue& body) {
  if (!m_token.IsValid()) {
    _RefreshToken();
  }

  auto headers = Headers {
      { std::string("Authorization"), m_token.GetToken() }
  };
  // TODO: this needs to be paged
  return MakeJsonRequest(uri, method, headers, body);
}

AWSError<Aws::Client::CoreErrors> GoogleApiClient::_BuildError(const std::shared_ptr<HttpResponse> &httpResponse) const {
  if (!httpResponse)
  {
    return AWSError<CoreErrors>(CoreErrors::NETWORK_CONNECTION, "", "Unable to connect to endpoint", true);
  }

  if (!httpResponse->GetResponseBody() || httpResponse->GetResponseBody().tellp() < 1)
  {
    Aws::StringStream ss;
    ss << "No response body.  Response code: " << static_cast< uint32_t >( httpResponse->GetResponseCode() );
    return AWSError<CoreErrors>(CoreErrors::UNKNOWN, "", ss.str(), false);
  }

  assert(httpResponse->GetResponseCode() != HttpResponseCode::OK);

  std::ostringstream os;
  os << httpResponse->GetResponseBody().rdbuf();
  /*
  //this is stupid, but gcc doesn't pick up the covariant on the dereference so we have to give it a little hint.
  JsonValue exceptionPayload(httpResponse->GetResponseBody());

  Aws::String message(exceptionPayload.ValueExists(MESSAGE_CAMEL_CASE) ? exceptionPayload.GetString(MESSAGE_CAMEL_CASE) :
                      exceptionPayload.ValueExists(MESSAGE_LOWER_CASE) ? exceptionPayload.GetString(MESSAGE_LOWER_CASE) : "");

  if (httpResponse->HasHeader(ERROR_TYPE_HEADER))
  {
    return GetErrorMarshaller()->Marshall(httpResponse->GetHeader(ERROR_TYPE_HEADER), message);
  }
  else if (exceptionPayload.ValueExists(TYPE))
  {
    return GetErrorMarshaller()->Marshall(exceptionPayload.GetString(TYPE), message);
  }
   */

  return AWSError<CoreErrors>(CoreErrors::UNKNOWN, "Error", os.str(), false);
}