#include "KRandom.h"

boost::thread_specific_ptr<std::default_random_engine> tls_random::m_ptr;