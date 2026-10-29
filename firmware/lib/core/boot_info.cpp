#include "boot_info.h"

#include <stdio.h>

namespace core {

const char* resetReasonName(ResetReason reason) {
    switch (reason) {
        case ResetReason::PowerOn: return "power-on";
        case ResetReason::External: return "external";
        case ResetReason::Software: return "software";
        case ResetReason::Panic: return "panic";
        case ResetReason::Watchdog: return "watchdog";
        case ResetReason::Brownout: return "brownout";
        case ResetReason::DeepSleep: return "deep-sleep";
        case ResetReason::Unknown: break;
    }
    return "unknown";
}

int formatBootBanner(char* buf, size_t len, const char* version, ResetReason reason) {
    return snprintf(buf, len, "SunTracker %s \xE2\x80\x94 reset: %s", version, resetReasonName(reason));
}

}  // namespace core
