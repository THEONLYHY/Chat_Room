#ifndef INCLUDE_COMMON_LOGGER_H_
#define INCLUDE_COMMON_LOGGER_H_

#include <memory>

#include "spdlog/logger.h"

class Logger {
 public:
  static bool Init();
  static std::shared_ptr<spdlog::logger> Get();

 private:
  static std::shared_ptr<spdlog::logger> logger_;
};

#endif  // INCLUDE_COMMON_LOGGER_H_