#pragma once
// Battery and mains watch (cycle 023). Pure logic: the driver reads two ADC pins (battery through a 1:2 divider,
// charger input through a 1:2 divider) and calls onSecond() once a second. Percent is a rough estimate from the
// resting voltage of a 1-cell Li-ion; it is wrong while charging or under load, and the panel says so.
#include <stdint.h>

namespace power {

enum class BatteryState : uint8_t { Unknown, Ok, Low, Critical };

enum Event : uint8_t {
    kNone = 0,
    kMainsLost = 1,
    kMainsRestored = 2,
    kBatteryLow = 4,
    kBatteryCritical = 8,
    kBatteryRecovered = 16,
};

struct Info {
    bool batteryKnown = false;  // the battery pin sees a battery
    float batteryV = 0.0f;
    int percent = -1;  // -1 unknown
    bool mainsKnown = false;  // the charger-input pin is wired
    bool mains = true;
    float mainsV = 0.0f;  // smoothed volts on the charger input (about 5 V when the mains is there)
    BatteryState state = BatteryState::Unknown;
};

constexpr float kLowV = 3.6f;        // warning
constexpr float kCriticalV = 3.4f;   // stop worrying about the schedule, the pack is almost empty
constexpr float kRecoverV = 3.75f;   // back to normal above this
constexpr float kMinPresentV = 2.5f; // below this the pin sees no battery
constexpr float kMainsOnV = 4.0f;    // charger input at or above: mains present
constexpr float kMainsOffV = 3.0f;   // at or below: mains gone (in between keeps the last state)
constexpr uint8_t kMainsDebounceS = 10;
constexpr uint8_t kBatteryDebounceS = 30;

// Percent from the resting cell voltage (linear between table points).
int percentFromVolts(float v);

class PowerMonitor {
public:
    // Call once a second. `chargerWired` false = the charger-input pin is not connected (no mains events).
    // Returns a bit mask of Event values (usually zero).
    uint8_t onSecond(float batteryV, float chargerV, bool chargerWired);
    const Info& info() const { return info_; }

private:
    Info info_;
    bool ema_init_ = false;
    bool mainsInit_ = false;
    uint8_t mainsCount_ = 0;
    bool mainsCand_ = true;
    uint8_t lowCount_ = 0, critCount_ = 0, okCount_ = 0;
};

}  // namespace power
