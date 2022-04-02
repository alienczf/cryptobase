#pragma once
#include <NanoLogCpp17.h>

#define LOGERR(...) NANO_LOG(ERR, __VA_ARGS__)
#define LOGWRN(...) NANO_LOG(WRN, __VA_ARGS__)
#define LOGINF(...) NANO_LOG(INF, __VA_ARGS__)
#define LOGDBG(...) NANO_LOG(DBG, __VA_ARGS__)
#define LOGTRC(...) NANO_LOG(TRC, __VA_ARGS__)

namespace ngh::log {
void SetupLogger(const std::string log_file) {
  NanoLog::preallocate();
  NanoLog::setLogLevel(NanoLog::LogLevels::INF);
  NanoLog::setLogFile(log_file.c_str());
}
}  // namespace ngh::log