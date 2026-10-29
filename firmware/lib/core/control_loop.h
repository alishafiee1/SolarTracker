#pragma once
#include <stdint.h>

namespace core {

constexpr uint32_t kControlPeriodMs = 100;

// One pass of the control logic every kControlPeriodMs (R6, cycle 001).
// Empty for now: sun position, motion and the state machine plug in here.
class ControlLoop {
public:
    void tick(uint32_t nowMs);
    uint32_t ticks() const { return ticks_; }
    uint32_t lastTickMs() const { return lastTickMs_; }

private:
    uint32_t ticks_ = 0;
    uint32_t lastTickMs_ = 0;
};

}  // namespace core
