#pragma once
// High-wind protection (cycle 006): wind speed from the anemometer pulses and the stow decision
// with time-based hysteresis. The rotor has one Hall pulse per revolution, so the speed comes from
// the time between pulses, not from counting pulses per second.

#include <stddef.h>
#include <stdint.h>

namespace wind {

constexpr uint32_t kDebounceUs = 5000;     // R1: pulses closer than this are noise
constexpr uint32_t kWindowUs = 3000000;    // R2: speed is taken over the last 3 s
constexpr size_t kMaxPulses = 64;

struct Config {
    float factorKmhPerHz = 6.8f;  // km/h per Hz of rotor rotation, calibrated on site
    float stowKmh = 40.0f;
    uint32_t stowDelayS = 5;
    float returnKmh = 30.0f;
    uint32_t returnMin = 15;
};

struct Result {
    bool stowStarted = false;
    bool stowEnded = false;
    float peakKmh = 0.0f;  // highest speed of the stow episode (valid with stowEnded / stowStarted)
};

class WindMonitor {
public:
    // Timestamp of one pulse in microseconds (any wrapping 32-bit clock). Order must be increasing.
    void feed(uint32_t timestampUs);

    // Call regularly (every control tick). nowUs must come from the same clock as the pulses.
    Result update(uint32_t nowMs, uint32_t nowUs, const Config& cfg);

    float speedKmh() const { return speedKmh_; }
    bool stow() const { return stow_; }
    // Seconds until the stow ends if the wind stays below the return speed (0 when not stowing).
    uint32_t returnRemainingS(uint32_t nowMs, const Config& cfg) const;

private:
    float computeSpeed(uint32_t nowUs, float factor);

    uint32_t pulses_[kMaxPulses] = {};
    size_t count_ = 0;

    float speedKmh_ = 0.0f;
    bool stow_ = false;
    bool above_ = false;
    uint32_t aboveSinceMs_ = 0;
    bool below_ = false;
    uint32_t belowSinceMs_ = 0;
    float peakKmh_ = 0.0f;
};

}  // namespace wind
