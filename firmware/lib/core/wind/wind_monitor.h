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
// Cycle 024: a reading that no real wind produces means the sensor or its wire is bad (noise, short, a loose cable).
constexpr float kImplausibleKmh = 120.0f;       // above this the speed is not believed
constexpr uint32_t kImplausibleHoldMs = 2000;   // ... for this long in a row raises the sensor fault
constexpr float kJumpKmh = 60.0f;               // a change this big between two updates counts as a jump
constexpr uint32_t kJumpWindowMs = 10000;       // three jumps inside this time raise the sensor fault
constexpr uint32_t kSensorCalmMs = 60000;       // the fault clears after this long with believable readings

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
    bool sensorFault = false;  // cycle 024: the readings became unbelievable
    bool sensorOk = false;     // ... and became believable again
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
    // Cycle 024: true while the readings are unbelievable (the stow logic ignores the sensor meanwhile).
    bool sensorFault() const { return sensorFault_; }
    // The user says the situation is fine (or the sensor is being tested): leaves the stow, forgets pulses and the sensor fault.
    // A real storm starts the stow again after the normal delay, so this is safe.
    void reset();
    // Seconds until the stow ends if the wind stays below the return speed (0 when not stowing).
    uint32_t returnRemainingS(uint32_t nowMs, const Config& cfg) const;

private:
    float computeSpeed(uint32_t nowUs, float factor);
    Result updateStow(uint32_t nowMs, const Config& cfg);

    uint32_t pulses_[kMaxPulses] = {};
    size_t count_ = 0;

    float speedKmh_ = 0.0f;
    bool stow_ = false;
    bool above_ = false;
    uint32_t aboveSinceMs_ = 0;
    bool below_ = false;
    uint32_t belowSinceMs_ = 0;
    float peakKmh_ = 0.0f;

    bool sensorFault_ = false;
    bool implausible_ = false;
    uint32_t implausibleSinceMs_ = 0;
    bool calm_ = false;
    uint32_t calmSinceMs_ = 0;
    float prevSpeedKmh_ = 0.0f;
    uint8_t jumps_ = 0;
    uint32_t jumpWindowStartMs_ = 0;
};

}  // namespace wind
