#include <chrono>
#include <memory>
#include <random>
#include <vector>
#include <unordered_set>

#include <boost/thread/tss.hpp>

class tls_random {
public:
    static std::default_random_engine* get() {
      if (!m_ptr.get()) {
        std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
        m_ptr.reset(new std::default_random_engine(now.time_since_epoch().count()));
      }
      return m_ptr.get();
    }

    /* For unit tests */
    static void seed_thread(std::default_random_engine::result_type seed) {
      m_ptr.reset(new std::default_random_engine(seed));
    }

private:
    static boost::thread_specific_ptr<std::default_random_engine> m_ptr;
};


template<typename T>
class k_random {
private:
    class k_random_iter_impl {
    public:
        virtual k_random_iter_impl& operator++() = 0;
        virtual const T& operator*() = 0;
    };

    // Uses an algorithm that is good if k is "sufficiently" smaller than n
    class k_random_iter_small : public k_random_iter_impl {
    public:
        k_random_iter_small(const std::vector<T> &items) :
            m_n(items.size()), m_items(items), m_dist(0, items.size() - 1) { }

        virtual k_random_iter_impl& operator++() {
          auto rnd = tls_random::get();
          size_t j;
          do {
            j = this->m_dist(*rnd);
          } while (this->m_selected.find(j) != this->m_selected.end());
          this->m_selected.insert(j);
          this->m_j = j;
          return *this;
        }

        virtual const T& operator*() {
          return m_items[m_j];
        }
    private:
        size_t m_n;
        size_t m_j;
        std::vector<T> m_items;
        std::unordered_set<size_t> m_selected;
        std::uniform_int_distribution<size_t> m_dist;
    };

    class k_random_iter_large : public k_random_iter_impl {
    public:
        k_random_iter_large(const std::vector<T> &items) :
            m_i(0), m_n(items.size()), m_pool(items) { }

        virtual k_random_iter_impl& operator++() {
          if (this->m_i < this->m_n) {
            auto rnd = tls_random::get();
            std::uniform_int_distribution<size_t> dist(0, this->m_n - this->m_i - 1);
            size_t j = dist(*rnd);
            this->m_curr = this->m_pool[j];
            this->m_pool[j] = this->m_pool[this->m_n - this->m_i - 1];
            this->m_i++;
          }
          return *this;
        }

        virtual const T& operator*() {
          return m_curr;
        }
    private:
        size_t m_i;
        size_t m_n;
        T m_curr;
        std::vector<T> m_pool;
    };


public:
    k_random(const std::vector<T> &items, const size_t k)
        : m_k(k), m_items(items) {
    }

    class k_random_iter {
    public:
        k_random_iter(const std::vector<T> &items, const size_t k)
            : m_i(0), m_n(items.size()) {
          size_t setSize = 21;
          if (k > 5) {
            setSize += ceil(log(k * 3) / log(4));
          }
          if (items.size() <= setSize) {
            this->m_impl = std::unique_ptr<k_random_iter_impl>(new k_random_iter_large(items));
          }
          else {
            this->m_impl = std::unique_ptr<k_random_iter_impl>(new k_random_iter_small(items));
          }
          ++(*this->m_impl);
        }

        k_random_iter(const size_t i)
            : m_i(i), m_n(0) {}

        const T operator*() {
          return **this->m_impl;
        }

        k_random_iter& operator++() {
          ++this->m_i;
          if (this->m_i < this->m_n) {
            ++(*this->m_impl);
          }
          return *this;
        }

        inline bool operator==(const k_random_iter& rhs) {
          return this->m_i == rhs.m_i;
        }

        inline bool operator!=(const k_random_iter& rhs) {
          return !(*this == rhs);
        }

    protected:
        size_t m_i, m_n;
        std::unique_ptr<k_random_iter_impl> m_impl;
    };

    k_random_iter begin() { return k_random_iter(this->m_items, this->m_k); }
    k_random_iter end() { return k_random_iter(m_k); }

private:
    size_t m_k;
    const std::vector<T> m_items;
};
