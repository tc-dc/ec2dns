#ifndef EC2DNS_ASYNCPAGER_H
#define EC2DNS_ASYNCPAGER_H

#include <functional>
#include <memory>
#include <thread>
#include <future>
#include <vector>

#include <googleapis/client/transport/http_transport.h>
#include <google/compute_api/compute_api.h>

#include "Instance.h"

template<class METHOD, class APIRESULT, class RESULT>
class AsyncPager {
  typedef std::vector<std::unique_ptr<Instance>> InstancesResult;
 public:
  AsyncPager(std::unique_ptr<METHOD> method, std::function<std::vector<RESULT>(const APIRESULT&)> projection)
      : m_method(std::move(method)),
        m_projection(projection)
  {
    this->_Run();
  }
  // Disallow move to ensure thread safety.
  AsyncPager(AsyncPager&&) = delete;

  std::future<std::vector<RESULT>> GetFuture() { return this->m_promise.get_future(); }

 private:
  void _Run() {
    this->m_method->ExecuteAsync(googleapis::NewCallback(this, &AsyncPager::_RequestComplete));
  }

  void _RequestComplete(googleapis::client::HttpRequest* request) {
    auto response = request->response();
    if (!response->ok()) {
      LOG(ERROR) << "Error calling API: " << response->status().ToString();
      this->m_promise.set_value(std::vector<RESULT>());
      return;
    }

    Json::Value data;
    google_compute_api::InstanceList l(&data);
    this->m_method->ParseResponse(response, &l);
    auto nextPage = l.get_next_page_token();
    auto ret = this->m_projection(l);
    std::move(ret.begin(), ret.end(), std::back_inserter(this->m_agg));

    if (!nextPage.empty()) {
      request->PrepareToReuse();
      this->m_method->set_page_token(nextPage.ToString());
      this->m_method->ExecuteAsync(googleapis::NewCallback(this, &AsyncPager::_RequestComplete));
    } else {
      this->m_promise.set_value(std::move(this->m_agg));
    }
  }

  std::promise<std::vector<RESULT>> m_promise;
  std::vector<RESULT> m_agg;
  std::unique_ptr<METHOD> m_method;
  std::function<std::vector<RESULT>(const APIRESULT&)> m_projection;
};

#endif //EC2DNS_ASYNCPAGER_H
