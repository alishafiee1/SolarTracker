// Cycle 004 — safe actuator motion (rodo/suntracker/cycles/004-safe-motion)
// One test per scenario of acceptance.feature plus the edge cases of spec.md.
#include <fakes.h>
#include <string.h>
#include <unity.h>

#include "motion_controller.h"

using hal::Dir;
using motion::Cmd;
using motion::Fault;
using motion::State;
using motion::Timing;

namespace motion {
// Lets a test try the forbidden thing (R8) that production code never does.
struct MotionControllerTestAccess {
    static bool writeDirection(MotionController& c, Dir d, uint32_t now) { return c.writeDirection(d, now); }
};
}  // namespace motion
using motion::MotionControllerTestAccess;

void setUp() {}
void tearDown() {}

struct Rig {
    sim::FakeTime time;
    sim::FakeRelays relays;
    sim::FakeEventLog log;
    motion::MotionController ctrl;
    float angle = 0.0f;
    float current = 0.0f;

    explicit Rig(const Timing& t = Timing()) : relays(&time), ctrl(relays, t, 0) { ctrl.setEventLog(&log); }

    void run(uint32_t ms, uint32_t stepMs = 10) {
        for (uint32_t t = 0; t < ms; t += stepMs) {
            time.advance(stepMs);
            ctrl.tick(time.nowMs(), angle, current);
        }
    }
    // Run until the condition holds (or fail after limitMs).
    template <typename F>
    uint32_t runUntil(F cond, uint32_t limitMs) {
        for (uint32_t t = 0; t < limitMs; t += 10) {
            if (cond()) return t;
            run(10);
        }
        TEST_FAIL_MESSAGE("condition not reached in time");
        return limitMs;
    }
    uint32_t now() const { return time.nowMs(); }
};

static const uint32_t kNever = 0xFFFFFFFFu;

// Time of the first CH1 on / off write at or after `after`.
static uint32_t nextPower(const Rig& r, bool on, uint32_t after) {
    bool prev = false;
    for (const auto& w : r.relays.writes()) {
        if (w.power == on && prev != on && w.atMs >= after) return w.atMs;
        prev = w.power;
    }
    return kNever;
}

// Time of the first CH2 change at or after `after`.
static uint32_t nextDirChange(const Rig& r, uint32_t after) {
    Dir prev = Dir::East;
    for (const auto& w : r.relays.writes()) {
        if (w.dir != prev && w.atMs >= after) return w.atMs;
        prev = w.dir;
    }
    return kNever;
}

// Bring a controller to "Running east" at a known time.
static void startRunning(Rig& r, Dir dir) {
    r.run(3100);  // R9 settle
    r.ctrl.request(Cmd::move(dir));
    r.runUntil([&] { return r.ctrl.running(); }, 1000);
}

// ---------------------------------------------------------------- R2
static void start_sequence_direction_then_power() {
    Rig r;
    r.run(5000);
    r.ctrl.request(Cmd::move(Dir::West));
    r.run(1000);
    const uint32_t tDir = nextDirChange(r, 5000);
    const uint32_t tPower = nextPower(r, true, 5000);
    TEST_ASSERT_NOT_EQUAL_UINT32(kNever, tDir);
    TEST_ASSERT_NOT_EQUAL_UINT32(kNever, tPower);
    TEST_ASSERT_UINT32_WITHIN(10, 200, tPower - tDir);  // 200 ms between choosing and powering
    TEST_ASSERT_TRUE(r.relays.direction() == Dir::West);
    TEST_ASSERT_EQUAL_INT(0, r.relays.violations());
}

// ---------------------------------------------------------------- R3 + R4
static void reversal_waits_and_never_switches_under_power() {
    Rig r;
    startRunning(r, Dir::East);
    r.run(2000);
    const uint32_t tRequest = r.now();
    r.ctrl.request(Cmd::move(Dir::West));
    r.run(10);
    // R3: CH1 off in the same tick.
    TEST_ASSERT_FALSE(r.relays.power());
    const uint32_t tStop = nextPower(r, false, tRequest);
    TEST_ASSERT_UINT32_WITHIN(10, tRequest, tStop);
    r.run(15000);
    const uint32_t tDir = nextDirChange(r, tStop);
    const uint32_t tPower = nextPower(r, true, tStop);
    TEST_ASSERT_NOT_EQUAL_UINT32(kNever, tPower);
    TEST_ASSERT_TRUE(tDir - tStop >= 3000);  // at least the 3 s settle (here the 10 s gap dominates)
    TEST_ASSERT_TRUE(tPower > tDir);
    TEST_ASSERT_TRUE(r.relays.direction() == Dir::West);
    // CH2 never changed while CH1 was on.
    TEST_ASSERT_EQUAL_INT(0, r.relays.violations());
    bool prevPower = false;
    Dir prevDir = Dir::East;
    for (const auto& w : r.relays.writes()) {
        if (w.dir != prevDir) TEST_ASSERT_FALSE_MESSAGE(prevPower, "CH2 changed while CH1 was on");
        prevPower = w.power;
        prevDir = w.dir;
    }
}

static void reversal_waits_until_the_angle_is_steady() {
    Timing t;
    t.minMoveGapMs = 1000;  // so the settle and the steady-angle rules are the ones that matter
    Rig r(t);
    startRunning(r, Dir::East);
    r.run(2000);
    r.ctrl.request(Cmd::move(Dir::West));
    r.run(10);
    const uint32_t tStop = nextPower(r, false, 0);
    // The panel coasts: the angle keeps changing for 5 s after the stop.
    while (r.now() < tStop + 5000) {
        r.angle += 0.05f;
        r.run(10);
    }
    r.run(4000);
    const uint32_t tPower = nextPower(r, true, tStop);
    TEST_ASSERT_NOT_EQUAL_UINT32(kNever, tPower);
    TEST_ASSERT_TRUE_MESSAGE(tPower >= tStop + 5000 + 1000, "reversed before the angle was steady for 1 s");
}

static void reversal_waits_for_zero_current_when_a_sensor_is_present() {
    Timing t;
    t.minMoveGapMs = 1000;
    t.currentPresent = true;
    Rig r(t);
    startRunning(r, Dir::East);
    r.run(2000);
    r.current = 3.0f;
    r.ctrl.request(Cmd::move(Dir::West));
    r.run(10);
    const uint32_t tStop = nextPower(r, false, 0);
    r.run(8000);  // coasting motor still draws current
    TEST_ASSERT_EQUAL_UINT32(kNever, nextPower(r, true, tStop));
    TEST_ASSERT_TRUE(r.ctrl.safetyWaitMs(r.now()) > 0);
    r.current = 0.0f;
    r.run(1000);
    TEST_ASSERT_NOT_EQUAL_UINT32(kNever, nextPower(r, true, tStop));
}

// ---------------------------------------------------------------- R5
static void gap_between_moves() {
    Rig r;
    startRunning(r, Dir::West);
    r.run(2000);
    r.ctrl.request(Cmd::stop());
    r.run(10);
    const uint32_t tEnd = nextPower(r, false, 0);
    r.run(4000);  // 4 s after the end
    r.ctrl.request(Cmd::move(Dir::West));
    r.run(5000);  // 9 s after the end: still no power
    TEST_ASSERT_EQUAL_UINT32(kNever, nextPower(r, true, tEnd));
    TEST_ASSERT_TRUE(r.ctrl.safetyWaitMs(r.now()) > 0);
    r.run(2000);
    const uint32_t tPower = nextPower(r, true, tEnd);
    TEST_ASSERT_NOT_EQUAL_UINT32(kNever, tPower);
    TEST_ASSERT_TRUE(tPower - tEnd >= 10000);
}

// ---------------------------------------------------------------- R6
static void move_timeout() {
    Rig r;
    startRunning(r, Dir::East);
    const uint32_t tStart = nextPower(r, true, 0);
    r.run(119000);
    TEST_ASSERT_TRUE(r.relays.power());
    r.run(2000);
    TEST_ASSERT_FALSE(r.relays.power());
    TEST_ASSERT_TRUE(r.ctrl.fault() == Fault::MoveTimeout);
    TEST_ASSERT_UINT32_WITHIN(30, tStart + 120000, nextPower(r, false, tStart));
    bool sawTimeout = false;
    for (const auto& e : r.log.events())
        if (e.first == hal::EventType::MoveEnd && strstr(e.second.c_str(), "reason=timeout")) sawTimeout = true;
    TEST_ASSERT_TRUE(sawTimeout);
}

// ---------------------------------------------------------------- R8
static void direction_change_under_power_is_refused() {
    Rig r;
    startRunning(r, Dir::East);
    const bool ok = MotionControllerTestAccess::writeDirection(r.ctrl, Dir::West, r.now());
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FALSE(r.relays.power());
    TEST_ASSERT_TRUE(r.relays.direction() == Dir::East);  // CH2 untouched
    TEST_ASSERT_EQUAL_INT(0, r.relays.violations());       // the guard stopped it before the relay layer
    TEST_ASSERT_TRUE(r.ctrl.fault() == Fault::InterlockViolation);
    bool logged = false;
    for (const auto& e : r.log.events())
        if (e.first == hal::EventType::Fault && strstr(e.second.c_str(), "interlock")) logged = true;
    TEST_ASSERT_TRUE(logged);
    // A violation blocks every move until the state machine clears it.
    r.ctrl.request(Cmd::move(Dir::East));
    r.run(20000);
    TEST_ASSERT_FALSE(r.relays.power());
    r.ctrl.clearFault();
    r.ctrl.request(Cmd::move(Dir::East));
    r.run(3000);
    TEST_ASSERT_TRUE(r.relays.power());
}

// ---------------------------------------------------------------- R9
static void first_move_is_not_immediate_after_boot() {
    Rig r;
    r.ctrl.request(Cmd::move(Dir::East));
    r.run(2990);
    TEST_ASSERT_FALSE(r.relays.power());
    TEST_ASSERT_UINT32_WITHIN(15, 10, r.ctrl.safetyWaitMs(r.now()));  // about 10 ms of the settle left
    r.run(400);
    TEST_ASSERT_TRUE(r.relays.power());
    TEST_ASSERT_TRUE(nextPower(r, true, 0) >= 3200);
}

static void safety_wait_counts_down() {
    Rig r;
    r.ctrl.request(Cmd::move(Dir::East));
    TEST_ASSERT_UINT32_WITHIN(20, 3000, r.ctrl.safetyWaitMs(0));
    r.run(1000);
    TEST_ASSERT_UINT32_WITHIN(30, 2000, r.ctrl.safetyWaitMs(r.now()));
    r.run(2500);
    TEST_ASSERT_EQUAL_UINT32(0, r.ctrl.safetyWaitMs(r.now()));
}

// ---------------------------------------------------------------- R10
static void emergency_stop_is_immediate() {
    Rig r;
    startRunning(r, Dir::West);
    r.run(500);
    r.ctrl.request(Cmd::move(Dir::West));
    r.ctrl.emergencyStop(r.now(), "test");
    TEST_ASSERT_FALSE(r.relays.power());
    TEST_ASSERT_TRUE(r.ctrl.state() == State::Settling);
    TEST_ASSERT_FALSE(r.ctrl.hasPending());
}

static void emergency_stop_while_selecting_keeps_power_off() {
    Rig r;
    r.run(5000);
    r.ctrl.request(Cmd::move(Dir::West));
    r.run(50);  // direction chosen, CH1 not yet on
    TEST_ASSERT_TRUE(r.ctrl.state() == State::Selecting);
    r.ctrl.emergencyStop(r.now(), "test");
    r.run(1000);
    TEST_ASSERT_EQUAL_UINT32(kNever, nextPower(r, true, 0));
}

// ---------------------------------------------------------------- edge cases
static void stop_during_selecting_never_powers() {
    Rig r;
    r.run(5000);
    r.ctrl.request(Cmd::move(Dir::East));
    r.run(50);
    TEST_ASSERT_TRUE(r.ctrl.state() == State::Selecting);
    r.ctrl.request(Cmd::stop());
    r.run(1000);
    TEST_ASSERT_EQUAL_UINT32(kNever, nextPower(r, true, 0));
    TEST_ASSERT_TRUE(r.ctrl.state() == State::Idle);
}

static void same_direction_request_while_running_is_ignored() {
    Rig r;
    startRunning(r, Dir::East);
    const size_t writes = r.relays.writes().size();
    for (int i = 0; i < 5; ++i) {
        r.ctrl.request(Cmd::move(Dir::East));
        r.run(100);
    }
    TEST_ASSERT_TRUE(r.relays.power());
    TEST_ASSERT_EQUAL_size_t(writes, r.relays.writes().size());
}

static void latest_request_wins_while_held() {
    Rig r;
    r.ctrl.request(Cmd::move(Dir::East));
    r.run(500);
    r.ctrl.request(Cmd::move(Dir::West));  // replaces the held east request
    r.run(4000);
    TEST_ASSERT_TRUE(r.relays.power());
    TEST_ASSERT_TRUE(r.relays.direction() == Dir::West);
}

// ---------------------------------------------------------------- R11
static void move_start_and_end_are_logged() {
    Rig r;
    startRunning(r, Dir::West);
    r.angle = 12.3f;
    r.run(1500);
    r.ctrl.request(Cmd::stop());
    r.run(20);
    bool start = false, end = false;
    for (const auto& e : r.log.events()) {
        if (e.first == hal::EventType::MoveStart && strstr(e.second.c_str(), "dir=west")) start = true;
        if (e.first == hal::EventType::MoveEnd && strstr(e.second.c_str(), "reason=stop") && strstr(e.second.c_str(), "angle=12.3")) end = true;
    }
    TEST_ASSERT_TRUE(start);
    TEST_ASSERT_TRUE(end);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(start_sequence_direction_then_power);
    RUN_TEST(reversal_waits_and_never_switches_under_power);
    RUN_TEST(reversal_waits_until_the_angle_is_steady);
    RUN_TEST(reversal_waits_for_zero_current_when_a_sensor_is_present);
    RUN_TEST(gap_between_moves);
    RUN_TEST(move_timeout);
    RUN_TEST(direction_change_under_power_is_refused);
    RUN_TEST(first_move_is_not_immediate_after_boot);
    RUN_TEST(safety_wait_counts_down);
    RUN_TEST(emergency_stop_is_immediate);
    RUN_TEST(emergency_stop_while_selecting_keeps_power_off);
    RUN_TEST(stop_during_selecting_never_powers);
    RUN_TEST(same_direction_request_while_running_is_ignored);
    RUN_TEST(latest_request_wins_while_held);
    RUN_TEST(move_start_and_end_are_logged);
    return UNITY_END();
}
