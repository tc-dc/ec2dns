#include "RequestThrottler.h"

#include <string>
#include <vector>

void RequestThrottler::Trim() {
  this->m_cache.Trim();
}

void RequestThrottler::OnMiss(const std::string &key, const std::string &clientAddr) {
  this->m_cache.Insert(key, clientAddr);
}

bool RequestThrottler::IsRequestThrottled(const std::string &clientAddr, const std::string &key) {
  // Never throttle a client requesting its own IP.
  if (clientAddr == key) {
    return false;
  }

  CacheEntry<std::string> entry;
  if (this->m_cache.TryGet(key, &entry)) {
    // It was found in the cache
    return entry.IsValid();
  } else {
    // Not found in the cache
    return false;
  }
}