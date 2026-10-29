#pragma once
#include <stddef.h>

namespace core {

// Why the chip restarted (R5, cycle 001). Drivers map esp_reset_reason() to this.
enum class ResetReason {
    PowerOn,
    External,
    Software,
    Panic,
    Watchdog,
    Brownout,
    DeepSleep,
    Unknown,
};

const char* resetReasonName(ResetReason reason);

// "SunTracker <version> — reset: <reason>" (R7). Returns the formatted length.
int formatBootBanner(char* buf, size_t len, const char* version, ResetReason reason);

}  // namespace core
