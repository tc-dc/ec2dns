#pragma once

#include <memory>
#include <mutex>

#include <json/json.h>

#include "server_http.hpp"

using namespace std::placeholders;

class Stat {
public:
    Stat(const Stat&) = delete;

    Stat(const std::string& name)
        : m_name(name), m_value(0) {
    }

    inline void Increment(const uint64_t amount=1) {
      m_value += amount;
    }

    const std::string GetName() {
      return this->m_name;
    }

    const uint64_t GetValue() {
      return this->m_value;
    }
private:
    const std::string m_name;
    std::atomic_uint_fast64_t m_value;
};

class StatsReceiver {
public:
  const std::vector<std::shared_ptr<Stat>> GetAllStats();
  std::shared_ptr<Stat> Create(const std::string& name);

private:
  std::vector<std::shared_ptr<Stat>> m_stats;
  std::mutex m_statsLock;
};

class StatsServer {
public:
    StatsServer(const StatsServer&) = delete;

    typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

    StatsServer(unsigned short port)
    {
      m_server = std::unique_ptr<HttpServer>(new HttpServer(port, 4));
      m_server->resource["^/stats$"]["GET"] = std::bind(&StatsServer::_RenderStats, this, _1, _2);
    }

    ~StatsServer() {
      this->Stop();
    }

    void SetStatsSource(const std::shared_ptr<StatsReceiver> statsReceiver) {
      this->m_stats = statsReceiver;
    }

    void Start();
    void Stop();

private:
    void _StartSync();
    void _RenderStats(HttpServer::Response& response, std::shared_ptr<HttpServer::Request> request);

    std::thread m_serverThread;
    std::unique_ptr<HttpServer> m_server;
    std::shared_ptr<StatsReceiver> m_stats;
};
