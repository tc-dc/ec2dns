#include "gce/InstanceCredentials.h"

#include <glog/logging.h>
#include <googleapis/client/data/data_reader.h>
#include <googleapis/base/callback-specializations.h>
#include <json/json.h>

using namespace googleapis::util;

const std::string InstanceCredentials::m_type = "instanceCredentials";

const std::string INSTANCE_METADATA_URI = "http://169.254.169.254/computeMetadata/v1/instance/service-accounts/default/token";
//const std::string INSTANCE_METADATA_URI = "http://localhost:8090/test";
const std::string METADATA_FLAVOR_HEADER = "metadata-flavor";
const std::string METADATA_FLAVOR_VALUE = "Google";

HttpRequest* InstanceCredentials::MakeRefreshRequest() {
  LOG(INFO) << "Making request for credentials to metadata endpoint";
  auto request = this->m_transport->NewHttpRequest(HttpRequest::GET);
  request->AddHeader(METADATA_FLAVOR_HEADER, METADATA_FLAVOR_VALUE);
  request->mutable_options()->set_timeout_ms(3 * 1000);
  request->mutable_options()->set_max_retries(2);
  request->set_url(INSTANCE_METADATA_URI);
  return request;
}

Status InstanceCredentials::Refresh() {
  std::unique_ptr<HttpRequest> request(MakeRefreshRequest());
  auto status = request->Execute();
  status = FinishRefresh(request.get());
  return status;
}

Status InstanceCredentials::FinishRefresh(HttpRequest *request) {
  auto status = request->response()->status();
  if (status.ok()) {
    Json::Value root;
    Json::Reader r;
    auto storage = request->response()->body_reader()->RemainderToString();
    if (!r.parse(storage, root)) {
      LOG(ERROR) << "Unable to parse json response, got: " << storage;
      return StatusInternalError("unable to parse json");
    }

    if (root.isMember("access_token") && root["access_token"].isString()) {
      m_token = "Bearer " + root["access_token"].asString();
      LOG(INFO) << "Got bearer token successfully from metadata";
    } else {
      LOG(ERROR) << "Unable to find access_token in response json";
      VLOG(1) << "Response JSON:\n" << storage;
    }
    if (root.isMember("expires_in") && root["expires_in"].isInt()) {
      auto expiresIn = root["expires_in"].asInt();
      m_expiresOn = std::chrono::system_clock::now() + std::chrono::seconds(expiresIn);
    }

  } else {
    LOG(ERROR) << "Unable to refresh from metadata endpoint, got error = " << status.ToString();
  }
  return status;
}

void InstanceCredentials::RefreshAsync(googleapis::Callback1<Status> *callback) {
  auto request = MakeRefreshRequest();
  request->DestroyWhenDone();
  HttpRequestCallback* cb = googleapis::NewCallback(this, &InstanceCredentials::FinishRefreshAsync, callback);
  request->ExecuteAsync(cb);
}

void InstanceCredentials::FinishRefreshAsync(googleapis::Callback1<Status> *callback, HttpRequest *request) {
  auto status = FinishRefresh(request);
  callback->Run(status);
}

Status InstanceCredentials::AuthorizeRequest(HttpRequest *request) {
  if (!m_token.empty()) {
    request->AddHeader(HttpRequest::HttpHeader_AUTHORIZATION, m_token);
  }
  return StatusOk();
}


Status InstanceCredentials::Load(DataReader *serialized_credential) {
  return StatusInternalError("Not implemented");
}

DataReader* InstanceCredentials::MakeDataReader() const {
  return nullptr;
}