#include "current_supervisor.h"

namespace current {

Event CurrentSupervisor::update(uint32_t nowMs, const Config& cfg, float currentA, bool powered, bool east, float angleDeg, float minDeg, float maxDeg) {
    if (!cfg.present) {  // R6: the module is not fitted: judge nothing
        wasPowered_ = powered;
        return Event::None;
    }
    if (powered && !wasPowered_) {
        poweredSinceMs_ = nowMs;
        zeroActive_ = overActive_ = false;
    }
    if (!powered && wasPowered_) {
        poweredOffSinceMs_ = nowMs;
        stuckActive_ = false;
    }
    wasPowered_ = powered;

    if (powered) {
        const uint32_t on = nowMs - poweredSinceMs_;
        if (on < kInrushMs) return Event::None;  // inrush: neither too much nor too little counts yet

        // R5: overcurrent.
        if (currentA > cfg.maxA) {
            if (!overActive_) {
                overActive_ = true;
                overSinceMs_ = nowMs;
            }
            if (nowMs - overSinceMs_ > kOverConfirmMs) return Event::Overcurrent;
        } else {
            overActive_ = false;
        }

        // R3: no current while powered.
        if (currentA < cfg.zeroA) {
            if (!zeroActive_) {
                zeroActive_ = true;
                zeroSinceMs_ = nowMs;
            }
            if (nowMs - zeroSinceMs_ >= kZeroConfirmMs) {
                zeroActive_ = false;
                const bool atEnd = east ? angleDeg <= minDeg + cfg.endMarginDeg : angleDeg >= maxDeg - cfg.endMarginDeg;
                return atEnd ? Event::LimitReached : Event::WireBreak;
            }
        } else {
            zeroActive_ = false;
        }
        return Event::None;
    }

    // R4: current after a stop command.
    if (nowMs - poweredOffSinceMs_ >= kAfterStopMs && currentA > cfg.stuckA) {
        if (!stuckActive_) {
            stuckActive_ = true;
            stuckSinceMs_ = nowMs;
        }
        if (nowMs - stuckSinceMs_ >= kStuckConfirmMs) {
            stuckActive_ = false;
            return Event::RelayStuck;
        }
    } else {
        stuckActive_ = false;
    }
    return Event::None;
}

}  // namespace current
