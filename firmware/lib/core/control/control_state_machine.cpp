#include "control_state_machine.h"

#include <math.h>
#include <stdio.h>

#include "sun_position.h"
#include "tracking.h"

namespace control {

const char* stateName(ActiveState s) {
    switch (s) {
        case ActiveState::Auto: return "Auto";
        case ActiveState::Night: return "Night";
        case ActiveState::Manual: return "Manual";
        case ActiveState::Wind: return "Wind";
        case ActiveState::Fault: return "Fault";
    }
    return "?";
}

const char* faultName(FaultCode f) {
    switch (f) {
        case FaultCode::None: return "none";
        case FaultCode::Stall: return "stall";
        case FaultCode::MoveTimeout: return "move-timeout";
        case FaultCode::AngleSensor: return "angle-sensor";
        case FaultCode::Interlock: return "interlock";
        case FaultCode::ResetLoop: return "reset-loop";
        case FaultCode::WireBreak: return "wire-break";
        case FaultCode::RelayStuck: return "relay-stuck";
        case FaultCode::Overcurrent: return "overcurrent";
    }
    return "?";
}

ControlStateMachine::ControlStateMachine(motion::MotionController& motion, const settings::Settings& cfg, hal::IEventLog* log)
    : motion_(motion), cfg_(cfg), log_(log), mode_(cfg.mode) {}

void ControlStateMachine::changeState(ActiveState next) {
    if (next == state_) return;
    if (log_) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%s->%s", stateName(state_), stateName(next));
        log_->append(hal::EventType::StateChange, buf);
    }
    if (state_ == ActiveState::Manual) manualActive_ = false;
    state_ = next;
}

void ControlStateMachine::latch(FaultCode code, uint32_t nowMs, const char* why) {
    if (fault_ != FaultCode::None) return;
    fault_ = code;
    if (log_) log_->append(hal::EventType::Fault, why ? why : faultName(code));
    if (code == FaultCode::AngleSensor || code == FaultCode::Interlock || code == FaultCode::ResetLoop || code == FaultCode::WireBreak ||
        code == FaultCode::RelayStuck || code == FaultCode::Overcurrent) {
        motion_.emergencyStop(nowMs, faultName(code));  // nothing safe to do with a blind or broken system
    }
}

// True while the panel still sits at the end where the jack's limit switch cut the power and `dir` pushes towards it.
bool ControlStateMachine::blockedByLimit(hal::Dir dir, float angleDeg) {
    if (!limitActive_) return false;
    if (dir != limitDir_ || fabsf(angleDeg - limitAngle_) > 2.0f) {
        limitActive_ = false;  // moved away or asked for the other way: the block ends
        return false;
    }
    return true;
}

void ControlStateMachine::stopMoving() {
    if (motion_.running() || motion_.hasPending()) motion_.request(motion::Cmd::stop());
}

CommandResult ControlStateMachine::command(Command cmd, uint32_t nowMs) {
    switch (cmd) {
        case Command::ModeAuto:
            mode_ = settings::Mode::Auto;
            manualActive_ = false;
            return CommandResult::Ok;
        case Command::ModeManual:
            mode_ = settings::Mode::Manual;
            return CommandResult::Ok;
        case Command::ResetFault:
            if (fault_ != FaultCode::None && log_) log_->append(hal::EventType::FaultReset, faultName(fault_));
            fault_ = FaultCode::None;
            stallAttempts_ = 0;
            stowFailed_ = false;
            motion_.clearFault();
            return CommandResult::Ok;
        case Command::ManualStop:
            manualActive_ = false;
            if (motion_.running() || motion_.hasPending()) motion_.request(motion::Cmd::stop());
            return CommandResult::Ok;
        case Command::KeepAlive:
            keepAliveMs_ = nowMs;
            return CommandResult::Ok;
        case Command::ManualEast:
        case Command::ManualWest:
            if (state_ == ActiveState::Wind) return CommandResult::WindActive;
            if (state_ == ActiveState::Fault) return CommandResult::FaultActive;
            if (mode_ != settings::Mode::Manual) return CommandResult::NeedsManualMode;
            manualActive_ = true;
            manualDir_ = cmd == Command::ManualEast ? hal::Dir::East : hal::Dir::West;
            keepAliveMs_ = nowMs;
            return CommandResult::Ok;
    }
    return CommandResult::Ok;
}

ActiveState ControlStateMachine::decide(const Inputs& in) {
    if (fault_ != FaultCode::None) {
        // One stow attempt is allowed after a stall or a move timeout when the wind asks for it (proposal step 7).
        const bool arm = fault_ == FaultCode::Stall || fault_ == FaultCode::MoveTimeout;
        if (in.windStow && arm && !stowFailed_) return ActiveState::Wind;
        return ActiveState::Fault;
    }
    if (in.windStow) return ActiveState::Wind;
    if (mode_ == settings::Mode::Manual) return ActiveState::Manual;
    if (in.clockValid) {
        const sun::SunPosition p = sun::position(in.utcEpoch, cfg_.latDeg, cfg_.lonDeg);
        if (sun::isNight(p)) return ActiveState::Night;
    }
    return ActiveState::Auto;
}

// Target angle for the automatic modes. Returns false when no safe target exists (and sets the reason).
bool ControlStateMachine::autoTarget(const Inputs& in, ActiveState s, float& target) {
    if (!in.calibrated) {
        inactiveReason_ = StatusText::NeedCalibration;
        return false;
    }
    if (s == ActiveState::Wind) {
        target = 0.0f;
        return true;
    }
    if (!in.clockValid) {
        inactiveReason_ = StatusText::ClockNotSet;
        return false;
    }
    const sun::SunPosition p = sun::position(in.utcEpoch, cfg_.latDeg, cfg_.lonDeg);
    if (s == ActiveState::Night) {
        target = 0.0f;
        if (cfg_.preDawnMinutes > 0 && p.azimuthDeg < 180.0) {  // before sunrise: face east early
            const sun::SunPosition soon = sun::position(in.utcEpoch + static_cast<int64_t>(cfg_.preDawnMinutes) * 60, cfg_.latDeg, cfg_.lonDeg);
            if (!sun::isNight(soon)) target = cfg_.minDeg;
        }
        return true;
    }
    target = static_cast<float>(tracking::target(tracking::rotation(p), cfg_.minDeg, cfg_.maxDeg));
    return true;
}

void ControlStateMachine::driveTo(float target, const Inputs& in) {
    const float err = target - in.angleDeg;
    if (motion_.running()) {
        const bool west = motion_.direction() == hal::Dir::West;
        const bool reached = west ? in.angleDeg >= target - kStopToleranceDeg : in.angleDeg <= target + kStopToleranceDeg;
        if (reached) motion_.request(motion::Cmd::stop());  // R2: ends within 1 degree or when passed
        return;
    }
    if (fabsf(err) > cfg_.moveThresholdDeg && !blockedByLimit(err > 0 ? hal::Dir::West : hal::Dir::East, in.angleDeg)) {
        motion_.request(motion::Cmd::move(err > 0 ? hal::Dir::West : hal::Dir::East));  // R2
    } else if (motion_.hasPending()) {
        motion_.request(motion::Cmd::stop());  // the need went away while the safety delay was running
    }
}

void ControlStateMachine::runManual(uint32_t nowMs, const Inputs& in) {
    if (manualActive_ && nowMs - keepAliveMs_ > kKeepAliveMs) {  // R4: the phone went away
        manualActive_ = false;
        if (log_) log_->append(hal::EventType::StateChange, "manual keep-alive lost");
    }
    if (manualActive_ && in.calibrated) {  // the angle limits only exist with a calibration
        if ((manualDir_ == hal::Dir::East && in.angleDeg <= cfg_.minDeg) || (manualDir_ == hal::Dir::West && in.angleDeg >= cfg_.maxDeg)) {
            manualActive_ = false;
        }
    }
    if (manualActive_ && blockedByLimit(manualDir_, in.angleDeg)) manualActive_ = false;  // the jack is at its end
    if (manualActive_) {
        motion_.request(motion::Cmd::move(manualDir_));
    } else {
        stopMoving();
    }
}

// R5: while running, the angle must progress at least 1 degree per check interval.
void ControlStateMachine::watchStall(uint32_t nowMs, const Inputs& in) {
    const bool running = motion_.running();
    if (running && !wasRunning_) {
        stallRefAngle_ = in.angleDeg;
        stallRefMs_ = nowMs;
        stallDir_ = motion_.direction();
    }
    wasRunning_ = running;
    if (!running || !in.calibrated || nowMs - stallRefMs_ < cfg_.stallCheckMs) return;
    // With the current module a jam means high current; no current at all is the end of the stroke or a broken wire,
    // which the current supervisor has already decided (module 009 R2/R3).
    if (cfg_.currentPresent && in.currentA < cfg_.zeroCurrentA) return;

    const float progress = stallDir_ == hal::Dir::West ? in.angleDeg - stallRefAngle_ : stallRefAngle_ - in.angleDeg;
    if (progress >= kStallProgressDeg) {
        stallAttempts_ = 0;  // a move that makes progress resets the counter
        stallRefAngle_ = in.angleDeg;
        stallRefMs_ = nowMs;
        return;
    }
    motion_.request(motion::Cmd::stop());
    ++stallAttempts_;
    stallRefMs_ = nowMs;
    // Manual moves and the single stow attempt after a fault are not retried.
    const bool noRetry = state_ == ActiveState::Manual || fault_ != FaultCode::None;
    if (state_ == ActiveState::Wind && fault_ != FaultCode::None) stowFailed_ = true;
    if (noRetry || stallAttempts_ >= kStallAttempts) {
        if (fault_ == FaultCode::None) latch(FaultCode::Stall, nowMs, "stall: no angle change after the move command");
    }
}

void ControlStateMachine::tick(uint32_t nowMs, const Inputs& in) {
    in_ = in;

    // Faults that come from outside this class.
    if (!in.angleOk) latch(FaultCode::AngleSensor, nowMs, "angle sensor not answering");
    if (in.resetLoop) latch(FaultCode::ResetLoop, nowMs, "repeated watchdog resets");
    if (motion_.fault() == motion::Fault::MoveTimeout) latch(FaultCode::MoveTimeout, nowMs, "move timeout");
    if (motion_.fault() == motion::Fault::InterlockViolation) latch(FaultCode::Interlock, nowMs, "interlock violation");

    // Optional current module: limit of the stroke, broken wire, stuck relay, overcurrent.
    {
        const current::Config cc{cfg_.currentPresent, cfg_.stallCurrentA, cfg_.maxCurrentA, cfg_.zeroCurrentA};
        switch (currentSup_.update(nowMs, cc, in.currentA, motion_.running(), motion_.direction() == hal::Dir::East, in.angleDeg, cfg_.minDeg, cfg_.maxDeg)) {
            case current::Event::LimitReached:
                motion_.request(motion::Cmd::stop());  // the jack's own limit switch cut the power: not a fault
                stallAttempts_ = 0;
                limitActive_ = true;
                limitDir_ = motion_.direction();
                limitAngle_ = in.angleDeg;
                manualActive_ = false;
                if (log_) log_->append(hal::EventType::StateChange, "end of stroke: limit reached");
                break;
            case current::Event::WireBreak: latch(FaultCode::WireBreak, nowMs, "wire break: no current while powered"); break;
            case current::Event::RelayStuck: latch(FaultCode::RelayStuck, nowMs, "relay stuck: current after the stop command"); break;
            case current::Event::Overcurrent: latch(FaultCode::Overcurrent, nowMs, "overcurrent"); break;
            case current::Event::None: break;
        }
    }

    changeState(decide(in));
    if (!in.windStow || fault_ == FaultCode::None) stowFailed_ = false;  // a new wind episode gets a new attempt

    hasTarget_ = false;
    inactiveReason_ = StatusText::Tracking;
    switch (state_) {
        case ActiveState::Fault:
            stopMoving();
            break;
        case ActiveState::Manual:
            runManual(nowMs, in);
            break;
        case ActiveState::Wind:
        case ActiveState::Night:
        case ActiveState::Auto:
            if (autoTarget(in, state_, target_)) {
                hasTarget_ = true;
                driveTo(target_, in);
            } else {
                stopMoving();  // no safe target: do not move on a guess
            }
            break;
    }

    watchStall(nowMs, in);
    motion_.tick(nowMs, in.angleDeg, in.currentA);
}

Status ControlStateMachine::status(uint32_t nowMs) const {
    Status s{};
    s.state = state_;
    s.fault = fault_;
    s.angleDeg = in_.angleDeg;
    s.targetDeg = hasTarget_ ? target_ : in_.angleDeg;
    s.moving = motion_.running();
    const bool dirOut = motion_.state() == motion::State::Selecting || motion_.state() == motion::State::Running;
    s.cmdDir = !dirOut ? 0 : motion_.direction() == hal::Dir::East ? 1 : 2;
    s.waitS = (motion_.safetyWaitMs(nowMs) + 999) / 1000;
    switch (state_) {
        case ActiveState::Fault:
            s.text = fault_ == FaultCode::Stall          ? StatusText::FaultStall
                     : fault_ == FaultCode::MoveTimeout  ? StatusText::FaultMoveTimeout
                     : fault_ == FaultCode::AngleSensor  ? StatusText::FaultAngleSensor
                     : fault_ == FaultCode::Interlock    ? StatusText::FaultInterlock
                     : fault_ == FaultCode::WireBreak    ? StatusText::FaultWireBreak
                     : fault_ == FaultCode::RelayStuck   ? StatusText::FaultRelayStuck
                     : fault_ == FaultCode::Overcurrent  ? StatusText::FaultOvercurrent
                                                         : StatusText::FaultResetLoop;
            break;
        case ActiveState::Wind: s.text = StatusText::WindStow; break;
        case ActiveState::Manual: s.text = StatusText::ManualMode; break;
        case ActiveState::Night:
        case ActiveState::Auto:
            if (!hasTarget_) s.text = inactiveReason_;
            else if (s.waitS > 0 && motion_.hasPending()) s.text = StatusText::WaitingSafety;
            else s.text = state_ == ActiveState::Night ? StatusText::NightStow : StatusText::Tracking;
            break;
    }
    return s;
}

}  // namespace control
