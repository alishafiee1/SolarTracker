#pragma once
// The only code that touches the relays (cycle 004, R1). Every mode (auto, manual,
// wind, night) asks it to move or stop; it enforces all timing interlocks.

#include <hal.h>

#include "safety_timing.h"

namespace motion {

enum class State : uint8_t { Idle, Selecting, Running, Settling };

enum class Fault : uint8_t {
    None,
    MoveTimeout,         // R6: a move lasted longer than maxMoveMs
    InterlockViolation,  // R8: something tried to change the direction under power
};

struct Cmd {
    enum class Kind : uint8_t { Move, Stop };
    Kind kind;
    hal::Dir dir;
    static Cmd move(hal::Dir d) { return {Kind::Move, d}; }
    static Cmd stop() { return {Kind::Stop, hal::Dir::East}; }
};

class MotionController {
public:
    // R9: starts in Settling with a full stopSettleMs, so the first move is never immediate.
    MotionController(hal::IRelays& relays, const Timing& timing, uint32_t nowMs);

    void setEventLog(hal::IEventLog* log) { log_ = log; }
    void setTiming(const Timing& timing) { timing_ = timing; }

    // Remember the latest request (R5: the latest wins). It is acted on in tick().
    void request(Cmd cmd);

    // R10: CH1 off right now, regardless of any timer.
    void emergencyStop(uint32_t nowMs, const char* reason);

    // Call every control tick. angleDeg = filtered panel angle, currentA = actuator current
    // (ignored unless Timing::currentPresent).
    void tick(uint32_t nowMs, float angleDeg, float currentA);

    State state() const { return state_; }
    hal::Dir direction() const { return direction_; }  // of the current or the last move
    bool running() const { return state_ == State::Running; }
    bool hasPending() const { return hasPending_; }
    // Milliseconds until the pending Move request is allowed to start (0 = no wait or nothing pending).
    uint32_t safetyWaitMs(uint32_t nowMs) const;
    uint32_t lastMoveEndMs() const { return lastMoveEndMs_; }

    Fault fault() const { return fault_; }
    // InterlockViolation blocks every move until cleared; MoveTimeout is informational for the state machine.
    void clearFault() { fault_ = Fault::None; }

private:
    friend struct MotionControllerTestAccess;

    bool canStart(uint32_t nowMs, hal::Dir dir) const;
    void updateStability(uint32_t nowMs, float angleDeg);
    bool writeDirection(hal::Dir dir, uint32_t nowMs);  // R8 guard
    void stopMove(uint32_t nowMs, const char* reason);
    void enterSettling(uint32_t nowMs);
    void log(hal::EventType type, const char* what) const;
    void logMove(hal::EventType type, const char* what, float extra) const;

    hal::IRelays& relays_;
    Timing timing_;
    hal::IEventLog* log_ = nullptr;

    State state_ = State::Settling;
    hal::Dir direction_ = hal::Dir::East;
    hal::Dir lastMoveDir_ = hal::Dir::East;
    bool everMoved_ = false;
    uint32_t stateSinceMs_ = 0;  // start of Selecting / Settling
    uint32_t moveStartMs_ = 0;
    uint32_t lastMoveEndMs_ = 0;

    bool hasPending_ = false;
    Cmd pending_ = Cmd::stop();

    float angleDeg_ = 0.0f;
    float stableRefDeg_ = 0.0f;
    uint32_t stableSinceMs_ = 0;
    bool stabilityInit_ = false;
    float currentA_ = 0.0f;

    Fault fault_ = Fault::None;
};

}  // namespace motion
