#include "RequestThrottler.h"

#include <string>
#include <vector>

void RequestThrottler::Trim() {
  std::lock_guard<std::mutex> lock(this->m_cacheLock);
  std::vector<std::string> toDelete;
  time_point<steady_clock> now = steady_clock::now();
  // Delete expired entries
  for (auto it = this->m_cache.begin(); it != this->m_cache.end(); ++it) {
    if (!it->second.IsValid(now)) {
      toDelete.push_back(it->first);
    }
  }
  for (auto it = toDelete.begin(); it != toDelete.end(); ++it) {
    this->m_cache.erase(*it);
  }
}

void RequestThrottler::OnMiss(const std::string &key, const std::string &clientAddr) {
  std::lock_guard<std::mutex> lock(this->m_cacheLock);
  auto expiresOn = std::chrono::steady_clock::now() + std::chrono::seconds(240);
  this->m_cache[key] = CacheEntry<std::string>(clientAddr, expiresOn);
}

bool RequestThrottler::IsRequestThrottled(const std::string &clientAddr, const std::string &key) {
  // Never throttle a client requesting its own IP.
  if (clientAddr == key) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(this->m_cacheLock);
    auto found = this->m_cache.find(key);

    // It was found in the cache
    if (found != this->m_cache.end()) {
      // But it's now expired
      if (!found->second.IsValid()) {
        this->m_cache.erase(found);
        return false;
      } else {
        // Still valid, throttle.
        return true;
      }
    } else {
      // Not found in the cache
      return false;
    }
  }
}