// Cycle 009 — current protection (rodo/suntracker/cycles/009-current-protection)
#include <stdio.h>
#include <unity.h>
#include <world.h>

#include "current_supervisor.h"

using control::ActiveState;
using control::FaultCode;
using current::Config;
using current::CurrentSupervisor;
using current::Event;

void setUp() {}
void tearDown() {}

static int64_t utc(int y, int m, int d, int hh, int mm) {
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int64_t days = 365LL * (y - 1970) + (y - 1969) / 4 + cum[m - 1] + (d - 1);
    if (m > 2 && (y % 4 == 0)) days += 1;
    return days * 86400 + hh * 3600 + mm * 60;
}
static double sunTarget(sim::World& w) {
    return tracking::target(tracking::rotation(sun::position(w.utc(), w.cfg().latDeg, w.cfg().lonDeg)), w.cfg().minDeg, w.cfg().maxDeg);
}

static Config fitted() {
    Config c;
    c.present = true;
    return c;
}

// Drive the supervisor with a constant situation for `ms` and return the first event.
static Event drive(CurrentSupervisor& s, uint32_t& now, uint32_t ms, float amps, bool powered, float angle = 0.0f, bool east = false) {
    const Config c = fitted();
    for (uint32_t t = 0; t < ms; t += 100) {
        now += 100;
        const Event e = s.update(now, c, amps, powered, east, angle, -45.0f, 45.0f);
        if (e != Event::None) return e;
    }
    return Event::None;
}

// ------------------------------------------------------------ R1-R5 unit level
static void normal_running_current_gives_no_event() {
    CurrentSupervisor s;
    uint32_t now = 0;
    TEST_ASSERT_TRUE(drive(s, now, 20000, 3.0f, true) == Event::None);
    TEST_ASSERT_TRUE(drive(s, now, 20000, 0.0f, false) == Event::None);  // and stopped
}

static void inrush_at_start_is_not_judged() {
    CurrentSupervisor s;
    uint32_t now = 0;
    drive(s, now, 1000, 0.0f, false);
    TEST_ASSERT_TRUE(drive(s, now, 300, 9.0f, true) == Event::None);  // 9 A in the first 300 ms
    TEST_ASSERT_TRUE(drive(s, now, 5000, 3.0f, true) == Event::None);
}

static void overcurrent_needs_more_than_300_ms() {
    CurrentSupervisor s;
    uint32_t now = 0;
    drive(s, now, 1000, 3.0f, true);
    TEST_ASSERT_TRUE(drive(s, now, 300, 6.0f, true) == Event::None);  // up to 300 ms is tolerated
    TEST_ASSERT_TRUE(drive(s, now, 200, 3.0f, true) == Event::None);  // and it recovers
    TEST_ASSERT_TRUE(drive(s, now, 600, 6.0f, true) == Event::Overcurrent);
}

static void no_current_at_the_end_of_the_stroke_is_a_limit_not_a_fault() {
    CurrentSupervisor s;
    uint32_t now = 0;
    drive(s, now, 1000, 3.0f, true, -43.0f, true);  // moving east, near the east end
    TEST_ASSERT_TRUE(drive(s, now, 3000, 0.0f, true, -43.0f, true) == Event::LimitReached);
}

static void no_current_in_the_middle_is_a_wire_break() {
    CurrentSupervisor s;
    uint32_t now = 0;
    drive(s, now, 1000, 3.0f, true, 10.0f, false);
    TEST_ASSERT_TRUE(drive(s, now, 3000, 0.0f, true, 10.0f, false) == Event::WireBreak);
}

static void a_short_dropout_is_not_a_wire_break() {
    CurrentSupervisor s;
    uint32_t now = 0;
    drive(s, now, 1000, 3.0f, true, 10.0f);
    TEST_ASSERT_TRUE(drive(s, now, 700, 0.0f, true, 10.0f) == Event::None);
    TEST_ASSERT_TRUE(drive(s, now, 3000, 3.0f, true, 10.0f) == Event::None);
}

static void current_after_the_stop_command_is_a_stuck_relay() {
    CurrentSupervisor s;
    uint32_t now = 0;
    drive(s, now, 2000, 3.0f, true);
    TEST_ASSERT_TRUE(drive(s, now, 400, 3.0f, false) == Event::None);       // the first 500 ms are the motor stopping
    TEST_ASSERT_TRUE(drive(s, now, 2000, 3.0f, false) == Event::RelayStuck);
}

static void a_module_that_is_not_fitted_judges_nothing() {
    CurrentSupervisor s;
    Config c;  // present = false
    uint32_t now = 0;
    for (int i = 0; i < 300; ++i) {
        now += 100;
        TEST_ASSERT_TRUE(s.update(now, c, 0.0f, true, false, 0.0f, -45.0f, 45.0f) == Event::None);  // powered with no current
    }
    TEST_ASSERT_FALSE(s.high(c, 9.0f));
}

// ------------------------------------------------------------ with the control stack
static const uint32_t kSeeds[3] = {5, 66, 777};

// Scenario 6: a welded CH1 contact. The motor keeps running after the stop command; the current shows it.
static void scenario_06_relay_fault() {
    for (uint32_t seed : kSeeds) {
        sim::World w(seed, utc(2027, 6, 21, 5, 0), settings::Mode::Manual);
        w.enableCurrentModule();
        w.calibrate();
        w.placePanel(0.0);
        w.run(4000);
        w.ctl().command(control::Command::ManualWest, w.time().nowMs());
        for (int i = 0; i < 5; ++i) {
            w.run(1000);
            w.ctl().command(control::Command::KeepAlive, w.time().nowMs());
        }
        w.act.relayStuck = true;  // the contacts weld while the panel is moving
        w.ctl().command(control::Command::ManualStop, w.time().nowMs());
        w.run(5000);
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::RelayStuck), static_cast<int>(w.ctl().fault()));
        TEST_ASSERT_FALSE(w.relays().power());  // the software did everything it could: CH1 is commanded off
        TEST_ASSERT_TRUE(w.actuatorCurrentA() > 1.0);  // and the actuator is still powered: this needs a person
        TEST_ASSERT_EQUAL(static_cast<int>(control::StatusText::FaultRelayStuck), static_cast<int>(w.ctl().status(w.time().nowMs()).text));
    }
}

// Scenario 7: the jack reaches its own limit; no fault.
static void scenario_07_end_of_stroke() {
    for (uint32_t seed : kSeeds) {
        sim::World w(seed, utc(2027, 6, 21, 5, 0), settings::Mode::Manual);
        w.enableCurrentModule();
        w.act.limitDeg = 44.0;  // the jack's own limit switch cuts the power at 44 degrees, just inside the software limit of 45
        w.calibrate();
        w.placePanel(30.0);
        w.run(4000);
        w.ctl().command(control::Command::ManualWest, w.time().nowMs());
        for (int i = 0; i < 40; ++i) {  // 14 degrees at 0.6 degrees per second take 24 s
            w.run(1000);
            w.ctl().command(control::Command::KeepAlive, w.time().nowMs());
        }
        TEST_ASSERT_TRUE(w.act.angleDeg >= 43.9);  // it did reach the end
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::None), static_cast<int>(w.ctl().fault()));  // and nobody complained
        TEST_ASSERT_FALSE(w.relays().power());
    }
}

// A broken wire in the middle of the stroke is a fault (the angle does not change either).
static void a_cut_actuator_cable_is_reported_as_a_wire_break_not_a_stall() {
    for (uint32_t seed : kSeeds) {
        sim::World w(seed, utc(2027, 6, 21, 5, 0));
        w.enableCurrentModule();
        w.calibrate();
        w.placePanel(sunTarget(w) + 20.0);
        w.act.wireBroken = true;
        w.run(60000);
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::WireBreak), static_cast<int>(w.ctl().fault()));
        TEST_ASSERT_FALSE(w.relays().power());
    }
}

// The same jam as scenario 1, now with high current: still three tries and a stall.
static void a_jammed_arm_with_high_current_is_still_a_stall() {
    for (uint32_t seed : kSeeds) {
        sim::World w(seed, utc(2027, 6, 21, 5, 0));
        w.enableCurrentModule();
        w.calibrate();
        w.placePanel(sunTarget(w) + 15.0);
        w.act.jammed = true;
        w.act.jamCurrentA = 4.8;  // between the stall level (4.5 A) and the overcurrent limit (5 A)
        w.run(80000);
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::Stall), static_cast<int>(w.ctl().fault()));
        TEST_ASSERT_EQUAL_INT(3, w.powerOns());
    }
}

// A hard jam that draws more than the limit is stopped at once as an overcurrent, without the three tries.
static void a_hard_jam_is_stopped_as_overcurrent() {
    for (uint32_t seed : kSeeds) {
        sim::World w(seed, utc(2027, 6, 21, 5, 0));
        w.enableCurrentModule();
        w.calibrate();
        w.placePanel(sunTarget(w) + 15.0);
        w.act.jammed = true;
        w.act.jamCurrentA = 6.5;
        w.run(30000);
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::Overcurrent), static_cast<int>(w.ctl().fault()));
        TEST_ASSERT_EQUAL_INT(1, w.powerOns());
    }
}

static void overcurrent_stops_the_actuator() {
    for (uint32_t seed : kSeeds) {
        sim::World w(seed, utc(2027, 6, 21, 5, 0));
        w.enableCurrentModule();
        w.calibrate();
        w.act.runCurrentA = 7.0;  // a failing motor draws too much
        w.placePanel(sunTarget(w) + 20.0);
        w.run(20000);
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::Overcurrent), static_cast<int>(w.ctl().fault()));
        TEST_ASSERT_FALSE(w.relays().power());
    }
}

// Without the module the same cut cable looks like a stall (angle only): the module changes the diagnosis, not the safety.
static void without_the_module_a_cut_cable_is_a_stall() {
    sim::World w(5, utc(2027, 6, 21, 5, 0));
    w.calibrate();
    w.placePanel(sunTarget(w) + 20.0);
    w.act.wireBroken = true;
    w.run(80000);
    TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::Stall), static_cast<int>(w.ctl().fault()));
}

// The current also gates a reversal (004 R4): a coasting actuator that still draws current delays it.
static void normal_tracking_with_the_module_has_no_false_faults() {
    for (uint32_t seed : kSeeds) {
        sim::World w(seed, utc(2027, 6, 21, 5, 0));  // 08:30 local: the target is inside the +-45 degree range and moving
        w.enableCurrentModule();
        w.calibrate();
        w.placePanel(sunTarget(w));
        for (int minute = 0; minute < 120; ++minute) {
            w.run(60000);
            if (minute % 3 == 0) w.setUtc(w.utc());  // no-op, keeps the loop body honest
        }
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::None), static_cast<int>(w.ctl().fault()));
        char msg[64];
        snprintf(msg, sizeof(msg), "%d moves, error %.2f deg", w.powerOns(), w.trackingError());
        TEST_MESSAGE(msg);
        TEST_ASSERT_TRUE_MESSAGE(w.powerOns() > 3, msg);
        TEST_ASSERT_TRUE_MESSAGE(fabs(w.trackingError()) < 6.0, msg);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(normal_running_current_gives_no_event);
    RUN_TEST(inrush_at_start_is_not_judged);
    RUN_TEST(overcurrent_needs_more_than_300_ms);
    RUN_TEST(no_current_at_the_end_of_the_stroke_is_a_limit_not_a_fault);
    RUN_TEST(no_current_in_the_middle_is_a_wire_break);
    RUN_TEST(a_short_dropout_is_not_a_wire_break);
    RUN_TEST(current_after_the_stop_command_is_a_stuck_relay);
    RUN_TEST(a_module_that_is_not_fitted_judges_nothing);
    RUN_TEST(scenario_06_relay_fault);
    RUN_TEST(scenario_07_end_of_stroke);
    RUN_TEST(a_cut_actuator_cable_is_reported_as_a_wire_break_not_a_stall);
    RUN_TEST(a_jammed_arm_with_high_current_is_still_a_stall);
    RUN_TEST(a_hard_jam_is_stopped_as_overcurrent);
    RUN_TEST(overcurrent_stops_the_actuator);
    RUN_TEST(without_the_module_a_cut_cable_is_a_stall);
    RUN_TEST(normal_tracking_with_the_module_has_no_false_faults);
    return UNITY_END();
}
