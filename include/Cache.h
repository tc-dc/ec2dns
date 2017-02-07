#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/shared_lock_guard.hpp>
#include <boost/thread/shared_mutex.hpp>

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
    CacheEntry<T> tmp;
    if (TryGet(key, &tmp)) {
      *value = tmp.GetItem();
      return true;
    } else {
      return false;
    }
  }

  bool TryGet(const std::string& key, CacheEntry<T>* value) {
    ReadLock lock(this->m_cacheLock);
    auto found = this->m_cache.find(key);
    if (found == this->m_cache.end()) {
      this->m_misses->Increment();
      return false;
    }
    else {
      *value = found->second;
      this->m_hits->Increment();
      return true;
    }
  }

  void Bulk(std::function<void(Cache<T>&)> fn) {
    WriteLock lock(this->m_cacheLock);
    fn(*this);
  }

  void Insert(const std::string& key, const T& value) {
    auto expiresOn = std::chrono::steady_clock::now() + std::chrono::seconds(this->m_defaultTimeout);
    this->Insert(key, value, expiresOn);
  }
  void Insert(const std::string& key, const T& value, const std::chrono::time_point<std::chrono::steady_clock> expiresOn) {
    WriteLock lock(this->m_cacheLock);
    this->InsertNoLock(key, value, expiresOn);
  }
  void InsertNoLock(const std::string& key, const T& value, const std::chrono::time_point<std::chrono::steady_clock> expiresOn) {
    this->m_cache[key] = CacheEntry<T>(value, expiresOn);
  }

  void Trim() {
    UpgradeableLock lock(this->m_cacheLock);
    time_point<steady_clock> now = steady_clock::now();

    std::vector<std::string> keys;
    // Find expired entries
    for (const auto& it : this->m_cache) {
      if (!it.second.IsValid(now)) {
        keys.push_back(it.first);
      }
    }

    // If there's anything to delete, upgrade the lock and do it.
    if (!keys.empty()) {
      Upgrade upgradeLock(lock);
      for (const auto& k : keys) {
        this->m_cache.erase(k);
      }
    }
  }

private:
  typedef boost::shared_lock_guard<boost::shared_mutex> ReadLock;
  typedef boost::upgrade_lock<boost::shared_mutex> UpgradeableLock;
  typedef boost::upgrade_to_unique_lock<boost::shared_mutex> Upgrade;
  typedef boost::lock_guard<boost::shared_mutex> WriteLock;

  unsigned int m_defaultTimeout;
  std::unordered_map<std::string, CacheEntry<T>> m_cache;
  boost::shared_mutex m_cacheLock;
  std::shared_ptr<Stat> m_hits, m_misses;
};