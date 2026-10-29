#include "motion_controller.h"

#include <math.h>
#include <stdio.h>

namespace motion {

namespace {

uint32_t elapsed(uint32_t now, uint32_t since) { return now - since; }  // wrap-safe for spans < 49 days
const char* dirName(hal::Dir d) { return d == hal::Dir::West ? "west" : "east"; }

uint32_t remaining(uint32_t spanMs, uint32_t elapsedMs) { return elapsedMs >= spanMs ? 0 : spanMs - elapsedMs; }

}  // namespace

MotionController::MotionController(hal::IRelays& relays, const Timing& timing, uint32_t nowMs)
    : relays_(relays), timing_(timing), stateSinceMs_(nowMs), stableSinceMs_(nowMs) {
    relays_.setPower(false);
}

void MotionController::request(Cmd cmd) {
    pending_ = cmd;
    hasPending_ = true;
}

void MotionController::log(hal::EventType type, const char* what) const {
    if (log_) log_->append(type, what);
}

void MotionController::logMove(hal::EventType type, const char* what, float extra) const {
    if (!log_) return;
    char buf[80];
    if (type == hal::EventType::MoveStart) {
        snprintf(buf, sizeof(buf), "dir=%s angle=%.1f", dirName(direction_), static_cast<double>(angleDeg_));
    } else {
        snprintf(buf, sizeof(buf), "reason=%s angle=%.1f ms=%lu", what, static_cast<double>(angleDeg_),
                 static_cast<unsigned long>(extra));
    }
    log_->append(type, buf);
}

void MotionController::enterSettling(uint32_t nowMs) {
    state_ = State::Settling;
    stateSinceMs_ = nowMs;
}

void MotionController::stopMove(uint32_t nowMs, const char* reason) {
    const uint32_t duration = elapsed(nowMs, moveStartMs_);
    relays_.setPower(false);  // R3: CH1 off first, CH2 stays
    lastMoveDir_ = direction_;
    lastMoveEndMs_ = nowMs;
    everMoved_ = true;
    logMove(hal::EventType::MoveEnd, reason, static_cast<float>(duration));
    enterSettling(nowMs);
}

void MotionController::emergencyStop(uint32_t nowMs, const char* reason) {
    hasPending_ = false;
    if (state_ == State::Running) {
        stopMove(nowMs, reason);
        return;
    }
    relays_.setPower(false);
    enterSettling(nowMs);
    log(hal::EventType::StateChange, reason);
}

bool MotionController::writeDirection(hal::Dir dir, uint32_t nowMs) {
    // R8: CH2 may only change while CH1 is off and nothing is running.
    if (relays_.power() || state_ == State::Running) {
        if (state_ == State::Running) {
            stopMove(nowMs, "interlock");
        } else {
            relays_.setPower(false);
            enterSettling(nowMs);
        }
        fault_ = Fault::InterlockViolation;
        log(hal::EventType::Fault, "interlock-violation: direction change under power");
        return false;
    }
    return relays_.setDirection(dir);
}

void MotionController::updateStability(uint32_t nowMs, float angleDeg) {
    if (!stabilityInit_) {
        stabilityInit_ = true;
        stableRefDeg_ = angleDeg;
        stableSinceMs_ = nowMs;
        return;
    }
    if (fabsf(angleDeg - stableRefDeg_) >= timing_.stableAngleDeg) {
        stableRefDeg_ = angleDeg;
        stableSinceMs_ = nowMs;
    }
}

bool MotionController::canStart(uint32_t nowMs, hal::Dir dir) const {
    if (fault_ == Fault::InterlockViolation) return false;
    if (everMoved_ && elapsed(nowMs, lastMoveEndMs_) < timing_.minMoveGapMs) return false;  // R5
    if (everMoved_ && dir != lastMoveDir_) {                                                // R4
        if (timing_.currentPresent && currentA_ >= timing_.zeroCurrentA) return false;
        if (elapsed(nowMs, stableSinceMs_) < timing_.reverseStableMs) return false;
    }
    return true;
}

uint32_t MotionController::safetyWaitMs(uint32_t nowMs) const {
    if (!hasPending_ || pending_.kind != Cmd::Kind::Move || state_ == State::Running || state_ == State::Selecting) return 0;
    uint32_t wait = 0;
    if (state_ == State::Settling) wait = remaining(timing_.stopSettleMs, elapsed(nowMs, stateSinceMs_));
    if (everMoved_) {
        const uint32_t gap = remaining(timing_.minMoveGapMs, elapsed(nowMs, lastMoveEndMs_));
        if (gap > wait) wait = gap;
        if (pending_.dir != lastMoveDir_) {
            const uint32_t stable = remaining(timing_.reverseStableMs, elapsed(nowMs, stableSinceMs_));
            if (stable > wait) wait = stable;
            if (timing_.currentPresent && currentA_ >= timing_.zeroCurrentA && wait == 0) wait = 1;
        }
    }
    return wait;
}

void MotionController::tick(uint32_t nowMs, float angleDeg, float currentA) {
    angleDeg_ = angleDeg;
    currentA_ = currentA;
    updateStability(nowMs, angleDeg);

    switch (state_) {
        case State::Running:
            if (hasPending_) {
                if (pending_.kind == Cmd::Kind::Stop) {
                    stopMove(nowMs, "stop");  // R3: CH1 off in this tick
                    hasPending_ = false;
                    return;
                }
                if (pending_.dir != direction_) {
                    stopMove(nowMs, "reverse");  // the reversal itself waits in Idle (R4)
                    return;
                }
                hasPending_ = false;  // same direction: already running
            }
            if (elapsed(nowMs, moveStartMs_) >= timing_.maxMoveMs) {  // R6
                fault_ = Fault::MoveTimeout;
                log(hal::EventType::Fault, "move-timeout");
                stopMove(nowMs, "timeout");
            }
            return;

        case State::Selecting:
            if (hasPending_) {
                if (pending_.kind == Cmd::Kind::Stop) {
                    hasPending_ = false;
                    state_ = State::Idle;  // CH1 never came on: nothing moved, no settle needed
                    log(hal::EventType::StateChange, "start-cancelled");
                    return;
                }
                if (pending_.dir != direction_) {
                    state_ = State::Idle;  // changed my mind before powering; re-evaluate below
                    break;
                }
                hasPending_ = false;
            }
            if (elapsed(nowMs, stateSinceMs_) >= timing_.relaySelectMs) {  // R2
                relays_.setPower(true);
                state_ = State::Running;
                moveStartMs_ = nowMs;
                logMove(hal::EventType::MoveStart, "", 0);
            }
            return;

        case State::Settling:
            if (elapsed(nowMs, stateSinceMs_) < timing_.stopSettleMs) {
                if (hasPending_ && pending_.kind == Cmd::Kind::Stop) hasPending_ = false;  // already stopped
                return;
            }
            state_ = State::Idle;
            break;

        case State::Idle:
            break;
    }

    if (state_ == State::Idle && hasPending_) {
        if (pending_.kind == Cmd::Kind::Stop) {
            hasPending_ = false;
            return;
        }
        if (canStart(nowMs, pending_.dir) && writeDirection(pending_.dir, nowMs)) {
            direction_ = pending_.dir;
            state_ = State::Selecting;
            stateSinceMs_ = nowMs;
            hasPending_ = false;
        }
    }
}

}  // namespace motion
