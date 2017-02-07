#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "Cache.h"
#include "CacheEntry.h"

class RequestThrottler {
public:
  RequestThrottler(std::shared_ptr<StatsReceiver> stats)
      : m_cache("throttler", stats, 240)
  {}

  bool IsRequestThrottled(const std::string& clientAddr, const std::string& key);
  void OnMiss(const std::string &key, const std::string &clientAddr);
  void Trim();
private:
  Cache<std::string> m_cache;
};