#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#include <NanoLogCpp17.h>
#pragma clang diagnostic pop
// TODO(ANY): portability

#define LOGERR(...) NANO_LOG(ERROR, __VA_ARGS__)
#define LOGWRN(...) NANO_LOG(WARNING, __VA_ARGS__)
#define LOGINF(...) NANO_LOG(NOTICE, __VA_ARGS__)
#define LOGDBG(...) NANO_LOG(DEBUG, __VA_ARGS__)
