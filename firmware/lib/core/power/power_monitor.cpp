#include "power_monitor.h"

namespace power {

int percentFromVolts(float v) {
    static const float volts[] = {4.20f, 4.10f, 4.00f, 3.90f, 3.80f, 3.70f, 3.60f, 3.50f, 3.40f, 3.30f};
    static const int pct[] = {100, 90, 78, 62, 48, 32, 18, 8, 3, 0};
    constexpr int n = sizeof(volts) / sizeof(volts[0]);
    if (v >= volts[0]) return 100;
    if (v <= volts[n - 1]) return 0;
    for (int i = 0; i + 1 < n; ++i) {
        if (v <= volts[i] && v > volts[i + 1]) {
            const float f = (v - volts[i + 1]) / (volts[i] - volts[i + 1]);
            return static_cast<int>(pct[i + 1] + f * (pct[i] - pct[i + 1]) + 0.5f);
        }
    }
    return 0;
}

uint8_t PowerMonitor::onSecond(float batteryV, float chargerV, bool chargerWired) {
    uint8_t ev = kNone;

    // Battery: the reading is smoothed (the actuator and the boost converter make it jump).
    if (batteryV >= kMinPresentV) {
        info_.batteryKnown = true;
        info_.batteryV = ema_init_ ? info_.batteryV * 0.8f + batteryV * 0.2f : batteryV;
        ema_init_ = true;
        info_.percent = percentFromVolts(info_.batteryV);

        const float v = info_.batteryV;
        lowCount_ = v < kLowV ? (lowCount_ < 255 ? lowCount_ + 1 : lowCount_) : 0;
        critCount_ = v < kCriticalV ? (critCount_ < 255 ? critCount_ + 1 : critCount_) : 0;
        okCount_ = v >= kRecoverV ? (okCount_ < 255 ? okCount_ + 1 : okCount_) : 0;

        if (info_.state == BatteryState::Unknown) {
            info_.state = v < kCriticalV ? BatteryState::Critical : v < kLowV ? BatteryState::Low : BatteryState::Ok;  // first reading: no message
        } else if (critCount_ >= kBatteryDebounceS && info_.state != BatteryState::Critical) {
            info_.state = BatteryState::Critical;
            ev |= kBatteryCritical;
        } else if (lowCount_ >= kBatteryDebounceS && info_.state == BatteryState::Ok) {
            info_.state = BatteryState::Low;
            ev |= kBatteryLow;
        } else if (okCount_ >= kBatteryDebounceS && info_.state != BatteryState::Ok) {
            info_.state = BatteryState::Ok;
            ev |= kBatteryRecovered;
        }
    } else {
        info_.batteryKnown = false;
        info_.percent = -1;
        info_.state = BatteryState::Unknown;
        ema_init_ = false;
        lowCount_ = critCount_ = okCount_ = 0;
    }

    // Mains: seen on the charger input. In the grey zone between the two limits the last state is kept.
    info_.mainsKnown = chargerWired;
    if (chargerWired) info_.mainsV = mainsInit_ ? info_.mainsV * 0.6f + chargerV * 0.4f : chargerV;
    if (chargerWired) {
        bool cand = mainsInit_ ? info_.mains : chargerV >= kMainsOnV;
        if (chargerV >= kMainsOnV) cand = true;
        else if (chargerV <= kMainsOffV) cand = false;
        if (!mainsInit_) {
            mainsInit_ = true;
            info_.mains = cand;  // the first reading is the starting point, no message
            mainsCand_ = cand;
            mainsCount_ = 0;
        } else if (cand != info_.mains) {
            mainsCount_ = cand == mainsCand_ ? (mainsCount_ < 255 ? mainsCount_ + 1 : mainsCount_) : 1;
            mainsCand_ = cand;
            if (mainsCount_ >= kMainsDebounceS) {
                info_.mains = cand;
                mainsCount_ = 0;
                ev |= cand ? kMainsRestored : kMainsLost;
            }
        } else {
            mainsCount_ = 0;
            mainsCand_ = info_.mains;
        }
    } else {
        mainsInit_ = false;
        mainsCount_ = 0;
    }
    return ev;
}

}  // namespace power
