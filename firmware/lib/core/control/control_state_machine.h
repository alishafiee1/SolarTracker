#pragma once
// The decision maker (cycle 005): picks the active mode each tick, turns it into a target angle,
// asks the MotionController to move or stop, detects stalls and latches faults.
// Priority, highest first: Fault > Wind > Manual > Night > Auto.

#include <hal.h>
#include <stdint.h>

#include "current_supervisor.h"
#include "motion_controller.h"
#include "settings.h"

namespace control {

enum class ActiveState : uint8_t { Auto, Night, Manual, Wind, Fault };

enum class FaultCode : uint8_t {
    None,
    Stall,         // the arm did not move after 3 tries
    MoveTimeout,   // a move took longer than the limit (004 R6)
    AngleSensor,   // accelerometer fault (003 R5)
    Interlock,     // direction interlock violated (004 R8)
    ResetLoop,     // repeated watchdog resets
    WireBreak,     // module 009: no current in the middle of the stroke
    RelayStuck,    // module 009: current keeps flowing after a stop
    Overcurrent,   // module 009: current above the limit
};

// Reasons shown to the user (the web panel maps each to its Persian text).
enum class StatusText : uint8_t {
    Tracking,
    NightStow,
    WindStow,
    ManualMode,
    FaultStall,
    FaultMoveTimeout,
    FaultAngleSensor,
    FaultInterlock,
    FaultResetLoop,
    WaitingSafety,
    ClockNotSet,
    NeedCalibration,
    FaultWireBreak,
    FaultRelayStuck,
    FaultOvercurrent,
};

enum class Command : uint8_t { ModeAuto, ModeManual, ManualEast, ManualWest, ManualStop, KeepAlive, ResetFault };
enum class CommandResult : uint8_t { Ok, NotInManualMode, WindActive, FaultActive, NeedsManualMode };

struct Inputs {
    float angleDeg = 0.0f;
    bool angleOk = true;
    bool calibrated = true;
    int64_t utcEpoch = 0;
    bool clockValid = true;
    bool windStow = false;
    bool resetLoop = false;
    float currentA = 0.0f;
};

struct Status {
    ActiveState state;
    FaultCode fault;
    StatusText text;
    float angleDeg;
    float targetDeg;   // meaningful in Auto, Night and Wind
    bool moving;
    uint32_t waitS;    // seconds until a pending move may start (safety delay)
    uint8_t cmdDir;    // direction output selected for a move: 0 none, 1 east, 2 west (set while the move starts and while it runs)
};

constexpr float kStopToleranceDeg = 1.0f;     // R2: a move ends within this of the target
constexpr float kStallProgressDeg = 1.0f;     // R5: the angle must move at least this much per check
constexpr uint8_t kStallAttempts = 3;         // R5
constexpr uint32_t kKeepAliveMs = 3000;       // R4

class ControlStateMachine {
public:
    ControlStateMachine(motion::MotionController& motion, const settings::Settings& cfg, hal::IEventLog* log = nullptr);

    CommandResult command(Command cmd, uint32_t nowMs);
    // Also ticks the MotionController, so the order "decide, then move" is fixed in one place.
    void tick(uint32_t nowMs, const Inputs& in);
    Status status(uint32_t nowMs) const;

    ActiveState state() const { return state_; }
    FaultCode fault() const { return fault_; }
    settings::Mode mode() const { return mode_; }

private:
    ActiveState decide(const Inputs& in);
    void latch(FaultCode code, uint32_t nowMs, const char* why);
    void changeState(ActiveState next);
    bool autoTarget(const Inputs& in, ActiveState s, float& target);
    void driveTo(float target, const Inputs& in);
    void runManual(uint32_t nowMs, const Inputs& in);
    void watchStall(uint32_t nowMs, const Inputs& in);
    void stopMoving();
    bool blockedByLimit(hal::Dir dir, float angleDeg);

    motion::MotionController& motion_;
    const settings::Settings& cfg_;
    hal::IEventLog* log_;

    ActiveState state_ = ActiveState::Auto;
    FaultCode fault_ = FaultCode::None;
    settings::Mode mode_;
    Inputs in_{};

    // Manual
    bool manualActive_ = false;
    hal::Dir manualDir_ = hal::Dir::East;
    uint32_t keepAliveMs_ = 0;

    // Target of the current mode
    float target_ = 0.0f;
    bool hasTarget_ = false;
    StatusText inactiveReason_ = StatusText::Tracking;

    // Stall watch
    bool wasRunning_ = false;
    float stallRefAngle_ = 0.0f;
    uint32_t stallRefMs_ = 0;
    hal::Dir stallDir_ = hal::Dir::East;
    uint8_t stallAttempts_ = 0;
    // The jack's own limit switch stopped it (module 009): do not push the same way again until the panel has left that end.
    bool limitActive_ = false;
    hal::Dir limitDir_ = hal::Dir::East;
    float limitAngle_ = 0.0f;
    current::CurrentSupervisor currentSup_;
    bool stowFailed_ = false;
};

const char* stateName(ActiveState s);
const char* faultName(FaultCode f);

}  // namespace control
