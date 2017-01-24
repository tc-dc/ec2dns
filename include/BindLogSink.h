#ifndef EC2DNS_BINDLOGSINK_H
#define EC2DNS_BINDLOGSINK_H

#include <glog/logging.h>
#include <glog/log_severity.h>
#include "dlz_minimal.h"

class BindLogSink : public google::LogSink {
 public:
  BindLogSink(log_t *logCb)
    : m_logCb(logCb)
  { }

  virtual void send(google::LogSeverity severity, const char* full_filename,
                    const char* base_filename, int line,
                    const struct ::tm* tm_time,
                    const char* message, size_t message_len) {
    this->m_logCb(
        this->log_levels[severity],
        "%c %s:%d] %s",
        google::LogSeverityNames[severity][0],
        base_filename,
        line,
        std::string(message, message_len).c_str());
  }

 private:
  log_t* m_logCb;
  int log_levels[google::NUM_SEVERITIES] = { ISC_LOG_INFO, ISC_LOG_WARNING, ISC_LOG_ERROR, ISC_LOG_CRITICAL };
};

#endif //EC2DNS_BINDLOGSINK_H
