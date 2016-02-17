#include "Stats.h"

const std::vector<std::shared_ptr<Stat>> StatsReceiver::GetAllStats() {
  std::lock_guard<std::mutex> lock(this->m_statsLock);
  return std::vector<std::shared_ptr<Stat>>(m_stats);
}

std::shared_ptr<Stat> StatsReceiver::Create(const std::string& name) {
  auto ptr = std::make_shared<Stat>(name);
  std::lock_guard<std::mutex> lock(this->m_statsLock);
  m_stats.push_back(ptr);
  return ptr;
}

void StatsServer::Start() {
  m_serverThread = std::thread(std::bind(&StatsServer::_StartSync, this));
}

void StatsServer::Stop() {
  this->m_server->stop();
}

void StatsServer::_StartSync() {
  this->m_server->start();
}

void StatsServer::_RenderStats(HttpServer::Response& response, std::shared_ptr<HttpServer::Request> request) {
  Aws::Utils::Json::JsonValue root;
  auto stats = m_stats->GetAllStats();
  for (auto &s : stats) {
    root.WithInt64(s->GetName(), s->GetValue());
  }
  auto resp = root.WriteReadable();
  response << "HTTP/1.1 200 OK\r\nContent-Length: " << resp.size() <<"\r\n\r\n" << resp;
}