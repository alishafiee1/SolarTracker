#include "wind_monitor.h"

namespace wind {

void WindMonitor::feed(uint32_t timestampUs) {
    if (count_ > 0 && static_cast<uint32_t>(timestampUs - pulses_[count_ - 1]) < kDebounceUs) return;  // R1
    if (count_ == kMaxPulses) {  // a full buffer means an absurd rate; keep the newest
        for (size_t i = 1; i < kMaxPulses; ++i) pulses_[i - 1] = pulses_[i];
        --count_;
    }
    pulses_[count_++] = timestampUs;
}

float WindMonitor::computeSpeed(uint32_t nowUs, float factor) {
    // Drop pulses older than the window (age is a wrapped difference; a pulse "from the future" is fresh).
    size_t first = 0;
    while (first < count_ && static_cast<int32_t>(nowUs - pulses_[first]) > static_cast<int32_t>(kWindowUs)) ++first;
    if (first > 0) {
        for (size_t i = first; i < count_; ++i) pulses_[i - first] = pulses_[i];
        count_ -= first;
    }
    if (count_ < 2) return 0.0f;  // fewer than two pulses in 3 s: slower than about 2 km/h
    const uint32_t spanUs = pulses_[count_ - 1] - pulses_[0];
    if (spanUs == 0) return 0.0f;
    const float hz = static_cast<float>(count_ - 1) * 1.0e6f / static_cast<float>(spanUs);
    return hz * factor;  // R2
}

Result WindMonitor::update(uint32_t nowMs, uint32_t nowUs, const Config& cfg) {
    Result r;
    speedKmh_ = computeSpeed(nowUs, cfg.factorKmhPerHz);

    if (speedKmh_ > cfg.stowKmh) {  // R3: strictly greater
        if (!above_) {
            above_ = true;
            aboveSinceMs_ = nowMs;
        }
    } else {
        above_ = false;
    }
    if (speedKmh_ > peakKmh_) peakKmh_ = speedKmh_;

    if (!stow_) {
        if (above_ && static_cast<uint32_t>(nowMs - aboveSinceMs_) > cfg.stowDelayS * 1000u) {
            stow_ = true;
            below_ = false;
            r.stowStarted = true;
            r.peakKmh = peakKmh_;
        } else if (!above_) {
            peakKmh_ = speedKmh_;  // no episode in progress: forget old gusts
        }
        return r;
    }

    // R4: return only after the wind stayed below the return speed for the whole delay.
    if (speedKmh_ < cfg.returnKmh) {
        if (!below_) {
            below_ = true;
            belowSinceMs_ = nowMs;
        }
        if (static_cast<uint32_t>(nowMs - belowSinceMs_) >= cfg.returnMin * 60000u) {
            stow_ = false;
            below_ = false;
            r.stowEnded = true;
            r.peakKmh = peakKmh_;
            peakKmh_ = speedKmh_;
        }
    } else {
        below_ = false;  // any sample at or above the return speed restarts the timer
    }
    return r;
}

uint32_t WindMonitor::returnRemainingS(uint32_t nowMs, const Config& cfg) const {
    if (!stow_) return 0;
    const uint32_t total = cfg.returnMin * 60u;
    if (!below_) return total;
    const uint32_t elapsed = static_cast<uint32_t>(nowMs - belowSinceMs_) / 1000u;
    return elapsed >= total ? 0 : total - elapsed;
}

}  // namespace wind
