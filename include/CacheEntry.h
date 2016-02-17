#pragma once

#include <chrono>

using namespace std::chrono;

template<class T>
class CacheEntry {
public:
    CacheEntry() { }

    CacheEntry(const T &item, const time_point<steady_clock> expiresOn):
        m_item(item), m_expiresOn(expiresOn) { }

    T GetItem() {
      return m_item;
    }

    bool IsValid() {
      return IsValid(steady_clock::now());
    }

    bool IsValid(const time_point<steady_clock> now) {
      return now < m_expiresOn;
    }
private:
    T m_item;
    time_point<steady_clock> m_expiresOn;
};