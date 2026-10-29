#pragma once
// Low supply voltage detection (cycle 007, R8): below the limit for 3 s -> one SupplyLow event.
#include <stdint.h>

namespace settings {

class SupplyWatch {
public:
    static constexpr uint32_t kLowSeconds = 3;
    static constexpr float kHysteresisV = 0.1f;

    // Call once per second. Returns true exactly once when the low condition is confirmed;
    // it re-arms after the voltage is back above lowV + hysteresis.
    bool onSecond(float volts, float lowV) {
        if (volts < lowV) {
            if (lowSeconds_ < 255) ++lowSeconds_;
            if (lowSeconds_ >= kLowSeconds && !reported_) {
                reported_ = true;
                return true;
            }
        } else if (volts >= lowV + kHysteresisV) {
            lowSeconds_ = 0;
            reported_ = false;
        }
        return false;
    }

private:
    uint8_t lowSeconds_ = 0;
    bool reported_ = false;
};

}  // namespace settings
