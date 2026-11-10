// Cycle 005 — control state machine and stall detection (rodo/suntracker/cycles/005-control-state-machine)
// A small actuator model closes the loop: the relays move the simulated panel at 0.6 deg/s.
#include <fakes.h>
#include <math.h>
#include <string.h>
#include <unity.h>

#include "control_state_machine.h"
#include "sun_position.h"
#include "tracking.h"

using control::ActiveState;
using control::Command;
using control::CommandResult;
using control::FaultCode;
using control::StatusText;

void setUp() {}
void tearDown() {}

static int64_t utc(int y, int m, int d, int hh, int mm) {
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int64_t days = 365LL * (y - 1970) + (y - 1969) / 4 + cum[m - 1] + (d - 1);
    if (m > 2 && (y % 4 == 0)) days += 1;
    return days * 86400 + hh * 3600 + mm * 60;
}

static float sunTarget(int64_t t, const settings::Settings& cfg = settings::Settings()) {
    return static_cast<float>(tracking::target(tracking::rotation(sun::position(t, cfg.latDeg, cfg.lonDeg)), cfg.minDeg, cfg.maxDeg));
}

struct Rig {
    sim::FakeTime time;
    sim::FakeRelays relays;
    sim::FakeEventLog log;
    settings::Settings cfg;
    motion::MotionController motion;
    control::ControlStateMachine sm;
    control::Inputs in;
    float angle = 0.0f;
    bool stuck = false;  // the arm is jammed: the relays switch but the panel does not move
    const float speedDegPerS = 0.6f;
    int powerOns = 0;

    explicit Rig(settings::Mode mode = settings::Mode::Auto, int64_t utcNow = utc(2027, 6, 21, 5, 0))
        : relays(&time), cfg(makeCfg(mode)), motion(relays, motion::Timing(), 0), sm(motion, cfg, &log) {
        in.utcEpoch = utcNow;
    }
    static settings::Settings makeCfg(settings::Mode mode) {
        settings::Settings c;
        c.mode = mode;
        return c;
    }

    void run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 100) {
            time.advance(100);
            in.utcEpoch += 0;  // the sun hardly moves in a test; tests that need it advance utc themselves
            const bool wasOn = relays.power();
            if (wasOn && !stuck) angle += (relays.direction() == hal::Dir::West ? 1.0f : -1.0f) * speedDegPerS * 0.1f;
            in.angleDeg = angle;
            sm.tick(time.nowMs(), in);
            if (!wasOn && relays.power()) ++powerOns;
        }
    }
    control::Status st() const { return sm.status(time.nowMs()); }
};

// The panel shows two symbols: "direction chosen" (command issued) and "motor running".
static void the_status_tells_the_chosen_direction_before_and_while_the_motor_runs() {
    Rig r;
    r.angle = sunTarget(r.in.utcEpoch) + 8.0f;  // beyond the threshold, on the east side of the target
    TEST_ASSERT_EQUAL_UINT8(0, r.st().cmdDir);
    bool chosenNotRunning = false, chosenAndRunning = false;
    for (int i = 0; i < 600; ++i) {
        r.run(100);
        const control::Status st = r.st();
        if (st.cmdDir == 1 && !st.moving) chosenNotRunning = true;
        if (st.cmdDir == 1 && st.moving) chosenAndRunning = true;
        TEST_ASSERT_NOT_EQUAL(2, st.cmdDir);  // never the other direction
    }
    TEST_ASSERT_TRUE(chosenNotRunning);
    TEST_ASSERT_TRUE(chosenAndRunning);
    TEST_ASSERT_EQUAL_UINT8(0, r.st().cmdDir);  // arrived: nothing is selected any more
}

// ------------------------------------------------------------ R2
static void moves_only_beyond_the_threshold_and_stops_within_one_degree() {
    Rig r;
    const float target = sunTarget(r.in.utcEpoch);
    r.angle = target + 4.0f;  // inside the 5 degree threshold
    r.run(30000);
    TEST_ASSERT_EQUAL_INT(0, r.powerOns);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, target + 4.0f, r.angle);

    r.angle = target + 8.0f;  // beyond it
    r.run(40000);
    TEST_ASSERT_TRUE(r.powerOns >= 1);
    TEST_ASSERT_FALSE(r.relays.power());
    TEST_ASSERT_FLOAT_WITHIN(1.2f, target, r.angle);
    TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Auto), static_cast<int>(r.sm.state()));
}

static void a_morning_of_tracking_stays_inside_six_degrees() {
    Rig r(settings::Mode::Auto, utc(2027, 6, 21, 3, 0));  // 06:30 local
    r.angle = sunTarget(r.in.utcEpoch);
    float worst = 0.0f;
    for (int minute = 0; minute < 180; ++minute) {
        r.in.utcEpoch += 60;
        for (int s = 0; s < 6; ++s) {
            r.run(10000);
            const float err = fabsf(r.angle - sunTarget(r.in.utcEpoch));
            if (err > worst) worst = err;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(worst < 6.0f, "tracking error reached 6 degrees");
    TEST_ASSERT_TRUE(r.powerOns >= 2 && r.powerOns < 60);
    TEST_ASSERT_EQUAL_INT(0, r.relays.violations());
}

// ------------------------------------------------------------ R3
static void night_stows_the_panel_horizontally() {
    Rig r(settings::Mode::Auto, utc(2027, 6, 21, 20, 0));
    r.angle = 30.0f;
    r.run(60000);
    TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Night), static_cast<int>(r.sm.state()));
    TEST_ASSERT_FLOAT_WITHIN(1.2f, 0.0f, r.angle);
    TEST_ASSERT_FALSE(r.relays.power());
    const int moves = r.powerOns;
    r.run(60000);
    TEST_ASSERT_EQUAL_INT(moves, r.powerOns);  // and it stays there
    TEST_ASSERT_EQUAL(static_cast<int>(StatusText::NightStow), static_cast<int>(r.st().text));
}

static void pre_dawn_faces_east_shortly_before_sunrise() {
    Rig r;
    r.cfg.preDawnMinutes = 30;
    int64_t t = utc(2027, 6, 21, 0, 0);
    while (sun::isNight(sun::position(t, r.cfg.latDeg, r.cfg.lonDeg))) t += 60;  // first minute of day
    r.angle = 0.0f;
    r.in.utcEpoch = t - 20 * 60;
    r.run(1000);
    TEST_ASSERT_EQUAL_FLOAT(r.cfg.minDeg, r.st().targetDeg);
    r.in.utcEpoch = t - 60 * 60;
    r.run(1000);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.st().targetDeg);
}

// ------------------------------------------------------------ R1 priorities
static void priority_order_fault_wind_manual_night_auto() {
    {  // day, auto
        Rig r;
        r.run(500);
        TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Auto), static_cast<int>(r.sm.state()));
    }
    {  // night beats auto
        Rig r(settings::Mode::Auto, utc(2027, 6, 21, 20, 0));
        r.run(500);
        TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Night), static_cast<int>(r.sm.state()));
    }
    {  // manual beats night
        Rig r(settings::Mode::Manual, utc(2027, 6, 21, 20, 0));
        r.run(500);
        TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Manual), static_cast<int>(r.sm.state()));
    }
    {  // wind beats manual
        Rig r(settings::Mode::Manual);
        r.in.windStow = true;
        r.run(500);
        TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Wind), static_cast<int>(r.sm.state()));
    }
    {  // fault beats wind and manual
        Rig r(settings::Mode::Manual);
        r.in.windStow = true;
        r.in.angleOk = false;
        r.run(500);
        TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Fault), static_cast<int>(r.sm.state()));
    }
}

static void wind_overrides_manual_mode_and_refuses_manual_commands() {
    Rig r(settings::Mode::Manual);
    r.angle = -30.0f;
    r.run(4000);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandResult::Ok), static_cast<int>(r.sm.command(Command::ManualWest, r.time.nowMs())));
    r.in.windStow = true;
    r.run(60000);
    TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Wind), static_cast<int>(r.sm.state()));
    TEST_ASSERT_FLOAT_WITHIN(1.2f, 0.0f, r.angle);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandResult::WindActive), static_cast<int>(r.sm.command(Command::ManualEast, r.time.nowMs())));
    TEST_ASSERT_EQUAL(static_cast<int>(StatusText::WindStow), static_cast<int>(r.st().text));
}

// ------------------------------------------------------------ R4 manual
static void manual_needs_manual_mode_and_a_keep_alive() {
    Rig r;  // auto mode
    TEST_ASSERT_EQUAL(static_cast<int>(CommandResult::NeedsManualMode), static_cast<int>(r.sm.command(Command::ManualWest, 0)));
    r.sm.command(Command::ModeManual, 0);
    r.run(4000);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandResult::Ok), static_cast<int>(r.sm.command(Command::ManualWest, r.time.nowMs())));
    for (int i = 0; i < 6; ++i) {  // the phone keeps sending keep-alives
        r.run(1000);
        r.sm.command(Command::KeepAlive, r.time.nowMs());
    }
    TEST_ASSERT_TRUE(r.relays.power());
    r.run(3500);  // no keep-alive for 3.5 s
    TEST_ASSERT_FALSE(r.relays.power());
}

static void manual_stop_command_stops_at_once() {
    Rig r(settings::Mode::Manual);
    r.run(4000);
    r.sm.command(Command::ManualEast, r.time.nowMs());
    r.run(2000);
    TEST_ASSERT_TRUE(r.relays.power());
    r.sm.command(Command::ManualStop, r.time.nowMs());
    r.run(200);
    TEST_ASSERT_FALSE(r.relays.power());
}

static void manual_stops_at_the_angle_limit() {
    Rig r(settings::Mode::Manual);
    r.angle = 43.5f;
    r.run(4000);
    r.sm.command(Command::ManualWest, r.time.nowMs());
    for (int i = 0; i < 12; ++i) {
        r.run(1000);
        r.sm.command(Command::KeepAlive, r.time.nowMs());
    }
    TEST_ASSERT_FALSE(r.relays.power());
    TEST_ASSERT_TRUE(r.angle <= r.cfg.maxDeg + 1.0f);
}

static void manual_works_without_a_calibration_but_auto_does_not() {
    Rig r(settings::Mode::Manual);
    r.in.calibrated = false;
    r.run(4000);
    r.sm.command(Command::ManualWest, r.time.nowMs());
    for (int i = 0; i < 4; ++i) {
        r.run(1000);
        r.sm.command(Command::KeepAlive, r.time.nowMs());
    }
    TEST_ASSERT_TRUE(r.relays.power());  // installers need this to do the calibration steps
    r.sm.command(Command::ManualStop, r.time.nowMs());
    r.sm.command(Command::ModeAuto, r.time.nowMs());
    r.angle = 20.0f;
    r.run(60000);
    TEST_ASSERT_EQUAL(static_cast<int>(StatusText::NeedCalibration), static_cast<int>(r.st().text));
    TEST_ASSERT_FALSE(r.relays.power());
}

// ------------------------------------------------------------ R7 clock
static void no_automatic_move_while_the_clock_is_not_set() {
    Rig r;
    r.in.clockValid = false;
    r.angle = 25.0f;
    r.run(60000);
    TEST_ASSERT_EQUAL_INT(0, r.powerOns);
    TEST_ASSERT_EQUAL(static_cast<int>(StatusText::ClockNotSet), static_cast<int>(r.st().text));
    r.in.clockValid = true;
    r.run(60000);
    TEST_ASSERT_TRUE(r.powerOns > 0);
}

// ------------------------------------------------------------ R5 stall
static void a_jammed_arm_gives_three_tries_then_a_fault() {
    Rig r;
    const float target = sunTarget(r.in.utcEpoch);
    r.angle = target + 15.0f;
    r.stuck = true;
    r.run(70000);
    TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Fault), static_cast<int>(r.sm.state()));
    TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::Stall), static_cast<int>(r.sm.fault()));
    TEST_ASSERT_EQUAL_INT(3, r.powerOns);
    TEST_ASSERT_FALSE(r.relays.power());
    TEST_ASSERT_EQUAL(static_cast<int>(StatusText::FaultStall), static_cast<int>(r.st().text));
    r.run(120000);
    TEST_ASSERT_EQUAL_INT(3, r.powerOns);  // no automatic move until the fault is reset
    bool logged = false;
    for (const auto& e : r.log.events())
        if (e.first == hal::EventType::Fault && strstr(e.second.c_str(), "stall")) logged = true;
    TEST_ASSERT_TRUE(logged);
}

static void fault_reset_lets_the_loop_try_again() {
    Rig r;
    const float target = sunTarget(r.in.utcEpoch);
    r.angle = target + 15.0f;
    r.stuck = true;
    r.run(70000);
    TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Fault), static_cast<int>(r.sm.state()));
    r.stuck = false;  // someone freed the arm
    TEST_ASSERT_EQUAL(static_cast<int>(CommandResult::Ok), static_cast<int>(r.sm.command(Command::ResetFault, r.time.nowMs())));
    r.run(60000);
    TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::None), static_cast<int>(r.sm.fault()));
    TEST_ASSERT_FLOAT_WITHIN(1.2f, target, r.angle);
    bool reset = false;
    for (const auto& e : r.log.events())
        if (e.first == hal::EventType::FaultReset) reset = true;
    TEST_ASSERT_TRUE(reset);
}

static void a_move_with_progress_does_not_count_as_a_stall() {
    Rig r;
    r.angle = sunTarget(r.in.utcEpoch) + 30.0f;  // 50 s of travel, several stall checks on the way
    r.run(120000);
    TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::None), static_cast<int>(r.sm.fault()));
    TEST_ASSERT_FLOAT_WITHIN(1.2f, sunTarget(r.in.utcEpoch), r.angle);
}

static void wind_gets_one_stow_attempt_even_after_a_stall_fault() {
    Rig r;
    r.angle = sunTarget(r.in.utcEpoch) + 15.0f;
    r.stuck = true;
    r.run(70000);
    TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::Stall), static_cast<int>(r.sm.fault()));
    r.stuck = false;
    r.in.windStow = true;
    r.run(80000);
    TEST_ASSERT_FLOAT_WITHIN(1.2f, 0.0f, r.angle);                                       // it stowed
    TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::Stall), static_cast<int>(r.sm.fault())); // the fault is still latched
    r.in.windStow = false;
    r.run(2000);
    TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Fault), static_cast<int>(r.sm.state()));
}

static void a_failed_stow_attempt_is_not_repeated() {
    Rig r;
    r.angle = sunTarget(r.in.utcEpoch) + 15.0f;
    r.stuck = true;
    r.run(70000);
    const int before = r.powerOns;
    r.in.windStow = true;  // still jammed
    r.run(120000);
    TEST_ASSERT_EQUAL_INT(before + 1, r.powerOns);
    TEST_ASSERT_FALSE(r.relays.power());
}

// ------------------------------------------------------------ faults from outside
static void a_sensor_fault_stops_everything_in_the_same_tick() {
    Rig r;
    r.angle = sunTarget(r.in.utcEpoch) + 20.0f;
    r.run(8000);
    TEST_ASSERT_TRUE(r.relays.power());
    r.in.angleOk = false;
    r.run(100);
    TEST_ASSERT_FALSE(r.relays.power());
    TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::AngleSensor), static_cast<int>(r.sm.fault()));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandResult::FaultActive), static_cast<int>(r.sm.command(Command::ManualWest, r.time.nowMs())));
    // The sensor comes back, but the fault stays until it is reset.
    r.in.angleOk = true;
    r.run(30000);
    TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Fault), static_cast<int>(r.sm.state()));
    r.sm.command(Command::ResetFault, r.time.nowMs());
    r.run(500);
    TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::None), static_cast<int>(r.sm.fault()));
}

static void a_move_timeout_from_the_motion_layer_becomes_a_fault() {
    Rig r;
    r.angle = sunTarget(r.in.utcEpoch) + 80.0f;  // 130 s of travel: longer than the 120 s limit
    r.cfg.minDeg = -45.0f;
    r.run(150000);
    TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::MoveTimeout), static_cast<int>(r.sm.fault()));
    TEST_ASSERT_FALSE(r.relays.power());
}

static void state_changes_are_logged() {
    Rig r;
    r.run(500);
    r.in.windStow = true;
    r.run(500);
    bool found = false;
    for (const auto& e : r.log.events())
        if (e.first == hal::EventType::StateChange && strstr(e.second.c_str(), "Auto->Wind")) found = true;
    TEST_ASSERT_TRUE(found);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(moves_only_beyond_the_threshold_and_stops_within_one_degree);
    RUN_TEST(the_status_tells_the_chosen_direction_before_and_while_the_motor_runs);
    RUN_TEST(a_morning_of_tracking_stays_inside_six_degrees);
    RUN_TEST(night_stows_the_panel_horizontally);
    RUN_TEST(pre_dawn_faces_east_shortly_before_sunrise);
    RUN_TEST(priority_order_fault_wind_manual_night_auto);
    RUN_TEST(wind_overrides_manual_mode_and_refuses_manual_commands);
    RUN_TEST(manual_needs_manual_mode_and_a_keep_alive);
    RUN_TEST(manual_stop_command_stops_at_once);
    RUN_TEST(manual_stops_at_the_angle_limit);
    RUN_TEST(manual_works_without_a_calibration_but_auto_does_not);
    RUN_TEST(no_automatic_move_while_the_clock_is_not_set);
    RUN_TEST(a_jammed_arm_gives_three_tries_then_a_fault);
    RUN_TEST(fault_reset_lets_the_loop_try_again);
    RUN_TEST(a_move_with_progress_does_not_count_as_a_stall);
    RUN_TEST(wind_gets_one_stow_attempt_even_after_a_stall_fault);
    RUN_TEST(a_failed_stow_attempt_is_not_repeated);
    RUN_TEST(a_sensor_fault_stops_everything_in_the_same_tick);
    RUN_TEST(a_move_timeout_from_the_motion_layer_becomes_a_fault);
    RUN_TEST(state_changes_are_logged);
    return UNITY_END();
}
