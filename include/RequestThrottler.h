#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "CacheEntry.h"

class RequestThrottler {
public:
    bool IsRequestThrottled(const std::string& clientAddr, const std::string& key);
    void OnMiss(const std::string &key, const std::string &clientAddr);
    void Trim();
private:
    std::mutex m_cacheLock;
    std::unordered_map<std::string, CacheEntry<std::string>> m_cache;
};