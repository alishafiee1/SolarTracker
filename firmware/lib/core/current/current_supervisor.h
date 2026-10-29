#pragma once
// Actuator current supervision (cycle 009, optional module B2 + C2). With the current transformer the
// controller can tell a jammed arm (high current), the end of the stroke or a broken wire (no current),
// a relay that stays stuck on (current after a stop) and an overcurrent apart.

#include <stdint.h>

namespace current {

struct Config {
    bool present = false;
    float stallA = 4.5f;      // R2: above this with no angle change = jammed
    float maxA = 5.0f;        // R5: above this for more than 300 ms = emergency stop
    float zeroA = 0.2f;       // R3: below this while powered = no current
    float stuckA = 0.3f;      // R4: above this while powered off = relay stuck
    float endMarginDeg = 5.0f;  // R3: "at the end of the stroke" = within this of min/max angle
};

enum class Event : uint8_t {
    None,
    LimitReached,  // R3: the jack's own limit switch cut the power at the end of the stroke (not a fault)
    WireBreak,     // R3: no current in the middle of the stroke
    RelayStuck,    // R4
    Overcurrent,   // R5
};

constexpr uint32_t kInrushMs = 300;       // current right after switching on is not judged
constexpr uint32_t kZeroConfirmMs = 1000; // R3: no current for this long
constexpr uint32_t kOverConfirmMs = 300;  // R5
constexpr uint32_t kStuckConfirmMs = 1000;// R4
constexpr uint32_t kAfterStopMs = 500;    // R4: the motor needs this long to stop drawing current

class CurrentSupervisor {
public:
    // One call per control tick. `powered` = CH1 on (MotionController::running()). `east` = direction of the
    // move. Returns one event at the moment something is decided; the caller reacts.
    Event update(uint32_t nowMs, const Config& cfg, float currentA, bool powered, bool east, float angleDeg, float minDeg, float maxDeg);

    // R2: true while the current is above the stall level (the state machine combines it with "angle unchanged").
    bool high(const Config& cfg, float currentA) const { return cfg.present && currentA > cfg.stallA; }

private:
    bool wasPowered_ = false;
    uint32_t poweredSinceMs_ = 0;
    uint32_t poweredOffSinceMs_ = 0;
    bool zeroActive_ = false;
    uint32_t zeroSinceMs_ = 0;
    bool overActive_ = false;
    uint32_t overSinceMs_ = 0;
    bool stuckActive_ = false;
    uint32_t stuckSinceMs_ = 0;
};

}  // namespace current
