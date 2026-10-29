#pragma once
#include <stdint.h>

namespace core {

// Fixed-period tick (R6, cycle 001). The first tick is one period after start.
// If the caller falls behind by more than a period, the schedule restarts from
// now instead of firing a burst of catch-up ticks.
class TickScheduler {
public:
    TickScheduler(uint32_t startMs, uint32_t periodMs) : periodMs_(periodMs), nextMs_(startMs + periodMs) {}

    bool due(uint32_t nowMs) {
        const int32_t late = static_cast<int32_t>(nowMs - nextMs_);
        if (late < 0) {
            return false;
        }
        nextMs_ = (static_cast<uint32_t>(late) >= periodMs_) ? nowMs + periodMs_ : nextMs_ + periodMs_;
        return true;
    }

private:
    uint32_t periodMs_;
    uint32_t nextMs_;
};

}  // namespace core
