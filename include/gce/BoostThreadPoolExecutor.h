#ifndef GCEDNS_BTPE_H
#define GCEDNS_BTPE_H

#include <boost/thread/thread_pool.hpp>
#include <googleapis/base/callback-types.h>
#include <googleapis/util/executor.h>


using namespace googleapis;

#define MAX_POOL_SIZE 10
#define MAX_CONCURRENCY 100

class BoostThreadPoolExecutor : public thread::Executor {
 public:
  BoostThreadPoolExecutor(uint32 pooledThreads, uint32 maxConcurrency)
      : m_poolSize(pooledThreads),
        m_maxConcurrency(maxConcurrency),
        m_tp(pooledThreads),
        m_executing(0)
  {}

  virtual void Add(googleapis::Closure* callback) {
    this->_TryAdd(callback, true);
  }

  virtual bool TryAdd(googleapis::Closure* callback) {
    return this->_TryAdd(callback, false);
  }

 private:
  bool _TryAdd(googleapis::Closure* callback, bool force = false) {
    // This isn't 100% correct wrt thread safety, eg we could have
    // slightly more than max_concurrency executing here.
    // However, its better to have a lock free path rather than 100%
    // correctness.
    uint32 nowExecuting = ++this->m_executing;
    if (nowExecuting >= this->m_maxConcurrency) {
      this->m_executing--;
      return false;
    }
    else {
      auto runIt = [callback, this]() {
        try {
          callback->Run();
        } catch(const std::exception& e) {
          LOG(ERROR) << "ThreadPoool caught exception: " << e.what();
        }
        this->m_executing--;
      };

      if (nowExecuting >= this->m_poolSize) {
        std::thread t(runIt);
        t.detach();
      } else {
        m_tp.submit(runIt);
      }
      return true;
    }
  }

  const uint32 m_poolSize;
  const uint32 m_maxConcurrency;
  boost::basic_thread_pool m_tp;
  std::atomic_uint_fast32_t m_executing;
};


#endif
