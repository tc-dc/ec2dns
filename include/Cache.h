#pragma once

#include <memory>
#include <unordered_map>
#include <string>

#include "CacheEntry.h"
#include "Stats.h"

template<class T>
class Cache {
public:
  Cache(const std::string& cacheName,
      std::shared_ptr<StatsReceiver> statsReceiver,
      unsigned int defaultTimeoutSec
  ) :
    m_defaultTimeout(defaultTimeoutSec),
    m_hits(statsReceiver->Create(cacheName + "_hits")),
    m_misses(statsReceiver->Create(cacheName + "_misses"))
  { }

  bool TryGet(const std::string& key, T* value) {
    std::lock_guard<std::mutex> lock(this->m_cacheLock);
    auto found = this->m_cache.find(key);
    if (found == this->m_cache.end()) {
      this->m_misses->Increment();
      return false;
    }
    else {
      *value = found->second.GetItem();
      this->m_hits->Increment();
      return true;
    }
  }

  std::unique_lock<std::mutex>&& GetLock() {
    return std::move(std::unique_lock<std::mutex>(this->m_cacheLock, std::try_to_lock));
  }

  void Insert(const std::string& key, const T& value) {
    auto expiresOn = std::chrono::steady_clock::now() + std::chrono::seconds(this->m_defaultTimeout);
    this->Insert(key, value, expiresOn);
  }
  void Insert(const std::string& key, const T& value, const std::chrono::time_point<std::chrono::steady_clock> expiresOn) {
    std::lock_guard<std::mutex> lock(this->m_cacheLock);
    this->InsertNoLock(key, value, expiresOn);
  }
  void InsertNoLock(const std::string& key, const T& value, const std::chrono::time_point<std::chrono::steady_clock> expiresOn) {
    this->m_cache[key] = CacheEntry<T>(value, expiresOn);
  }

  void Trim() {
    std::lock_guard<std::mutex>(this->m_cacheLock);
    std::vector<std::string> toDelete;
    time_point<steady_clock> now = steady_clock::now();
    // Delete expired entries
    for (auto& it : this->m_cache) {
      if (!it.second.IsValid(now)) {
        toDelete.push_back(it.first);
      }
    }
    for (auto& it : toDelete) {
      this->m_cache.erase(it);
    }
  }

private:
  unsigned int m_defaultTimeout;
  std::unordered_map<std::string, CacheEntry<T>> m_cache;
  std::mutex m_cacheLock;
  std::shared_ptr<Stat> m_hits, m_misses;
};