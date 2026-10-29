// Cycle 013 — the proposal's 13 acceptance scenarios on the simulator
// (rodo/suntracker/cycles/013-simulation-acceptance). The real control stack runs against a model of the
// actuator, the accelerometer and the anemometer; every scenario runs with 3 different seeds.
#include <stdio.h>
#include <unity.h>
#include <world.h>

using control::ActiveState;
using control::Command;
using control::FaultCode;
using sim::World;

void setUp() {}
void tearDown() {}

static const uint32_t kSeeds[3] = {11, 222, 3333};

static int64_t utc(int y, int m, int d, int hh, int mm) {
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int64_t days = 365LL * (y - 1970) + (y - 1969) / 4 + cum[m - 1] + (d - 1);
    if (m > 2 && (y % 4 == 0)) days += 1;
    return days * 86400 + hh * 3600 + mm * 60;
}

static double sunTarget(World& w) {
    return tracking::target(tracking::rotation(sun::position(w.utc(), w.cfg().latDeg, w.cfg().lonDeg)), w.cfg().minDeg, w.cfg().maxDeg);
}

static bool logHas(World& w, hal::EventType type, const char* text) {
    for (const auto& e : w.log().events())
        if (e.first == type && e.second.find(text) != std::string::npos) return true;
    return false;
}

// 1 ---------------------------------------------------------------------------- jammed arm
static void scenario_01_stall() {
    for (uint32_t seed : kSeeds) {
        World w(seed, utc(2027, 6, 21, 5, 0));
        w.calibrate();
        w.placePanel(sunTarget(w) + 12.0 + (seed % 5));
        w.act.jammed = true;
        w.run(75000);
        TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Fault), static_cast<int>(w.ctl().state()));
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::Stall), static_cast<int>(w.ctl().fault()));
        TEST_ASSERT_EQUAL_INT(3, w.powerOns());  // three tries ...
        TEST_ASSERT_FALSE(w.relays().power());   // ... then the power stays off
        TEST_ASSERT_TRUE(logHas(w, hal::EventType::Fault, "stall"));
        w.run(120000);
        TEST_ASSERT_EQUAL_INT(3, w.powerOns());
    }
}

// 2 ---------------------------------------------------------------------------- strong wind
static void scenario_02_wind() {
    for (uint32_t seed : kSeeds) {
        World w(seed, utc(2027, 6, 21, 5, 0));
        w.calibrate();
        w.placePanel(sunTarget(w));
        w.windKmh = [](uint32_t ms) { return ms < 60000 ? 8.0 : (ms < 100000 ? 45.0 : 20.0); };
        w.run(59000);
        TEST_ASSERT_FALSE(w.wind().stow());
        w.run(9000);  // 5 s above the limit have passed
        TEST_ASSERT_TRUE(w.wind().stow());
        w.run(90000);
        TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Wind), static_cast<int>(w.ctl().state()));
        TEST_ASSERT_TRUE(fabs(w.act.angleDeg) < 1.6);  // flat
        // 14 minutes of calm after the storm: still stowed; after 15: tracking again.
        w.run(100000 + 14 * 60 * 1000 - w.elapsedMs());
        TEST_ASSERT_TRUE(w.wind().stow());
        w.run(2 * 60 * 1000);
        TEST_ASSERT_FALSE(w.wind().stow());
        w.run(150000);
        TEST_ASSERT_TRUE(fabs(w.trackingError()) < 6.0);
    }
}

// 3 ---------------------------------------------------------------------------- night
static void scenario_03_night() {
    for (uint32_t seed : kSeeds) {
        World w(seed, utc(2027, 6, 21, 20, 0));  // after sunset
        w.calibrate();
        w.placePanel(35.0 + seed % 7);
        w.run(90000);
        TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Night), static_cast<int>(w.ctl().state()));
        TEST_ASSERT_TRUE(fabs(w.act.angleDeg) < 1.6);
        const int moves = w.powerOns();
        w.run(30 * 60 * 1000);
        TEST_ASSERT_EQUAL_INT(moves, w.powerOns());  // stays flat until sunrise
    }
}

// 4 ---------------------------------------------------------------------------- mains power cut
static void scenario_04_power_cut() {
    for (uint32_t seed : kSeeds) {
        World w(seed, utc(2027, 6, 21, 5, 0));
        settings::Settings s = w.cfg();
        s.moveThresholdDeg = 7.0f;
        TEST_ASSERT_TRUE(w.store().save(s));
        w.cfg() = s;
        w.calibrate();
        w.placePanel(sunTarget(w) + 9.0);
        w.run(120000);
        const int64_t before = w.utc();
        w.powerCut(30 * 60 * 1000);
        TEST_ASSERT_EQUAL_INT(2, w.bootCount());
        TEST_ASSERT_EQUAL_INT64(before + 1800, w.utc());                // the battery kept the time
        TEST_ASSERT_EQUAL_FLOAT(7.0f, w.cfg().moveThresholdDeg);         // settings survived
        TEST_ASSERT_TRUE(w.angle().calibrated());                        // so did the angle calibration
        TEST_ASSERT_FALSE(w.relays().power());                           // relays off after the restart
        TEST_ASSERT_TRUE(logHas(w, hal::EventType::PowerUp, "after cut"));
        w.run(90000);
        TEST_ASSERT_TRUE(fabs(w.trackingError()) < 7.0 + 1.0);           // and it tracks again
    }
}

// 5 ---------------------------------------------------------------------------- accelerometer fault
static void scenario_05_angle_sensor_fault() {
    for (uint32_t seed : kSeeds) {
        World w(seed, utc(2027, 6, 21, 5, 0));
        w.calibrate();
        w.placePanel(sunTarget(w) + 25.0);
        w.run(10000);
        TEST_ASSERT_TRUE(w.relays().power());  // moving
        w.sensorUnplugged = true;
        w.run(1000);
        TEST_ASSERT_FALSE(w.relays().power());  // stopped within a second
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::AngleSensor), static_cast<int>(w.ctl().fault()));
        const double stopped = w.act.angleDeg;
        w.run(60000);
        TEST_ASSERT_FLOAT_WITHIN(0.5f, static_cast<float>(stopped), static_cast<float>(w.act.angleDeg));  // it never moved again
    }
}

// 6 and 7 need the optional current module (cycle 009) ------------------------
static void scenario_06_relay_fault() { TEST_IGNORE_MESSAGE("needs module 009 (current protection): stuck relay is only visible as current"); }
static void scenario_07_end_of_stroke() { TEST_IGNORE_MESSAGE("needs module 009 (current protection): limit versus stall is told apart by the current"); }

// 8 ---------------------------------------------------------------------------- hotspot off
static void scenario_08_wifi_off_does_not_matter() {
    // The control stack in World has no web server at all: nothing in the control path depends on the hotspot.
    for (uint32_t seed : kSeeds) {
        World w(seed, utc(2027, 6, 21, 4, 0));
        w.calibrate();
        w.placePanel(sunTarget(w));
        double worst = 0;
        for (int minute = 0; minute < 40; ++minute) {
            w.run(60000);
            if (fabs(w.trackingError()) > worst) worst = fabs(w.trackingError());
        }
        TEST_ASSERT_TRUE(worst < 6.0);
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::None), static_cast<int>(w.ctl().fault()));
    }
}

// 9 ---------------------------------------------------------------------------- fault reset
static void scenario_09_fault_reset() {
    for (uint32_t seed : kSeeds) {
        World w(seed, utc(2027, 6, 21, 5, 0));
        w.calibrate();
        w.placePanel(sunTarget(w) + 14.0);
        w.act.jammed = true;
        w.run(75000);
        TEST_ASSERT_EQUAL(static_cast<int>(ActiveState::Fault), static_cast<int>(w.ctl().state()));
        w.act.jammed = false;
        w.ctl().command(Command::ResetFault, w.time().nowMs());
        w.run(60000);
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::None), static_cast<int>(w.ctl().fault()));
        TEST_ASSERT_TRUE(fabs(w.trackingError()) < 1.6);
    }
}

// 10 --------------------------------------------------------------------------- offline update
static void scenario_10_offline_update() { TEST_IGNORE_MESSAGE("needs module 010 (offline update)"); }

// 11 --------------------------------------------------------------------------- a whole sunny day
static void scenario_11_tracking_accuracy() {
    const int64_t days[4] = {utc(2027, 3, 20, 0, 0), utc(2027, 6, 21, 0, 0), utc(2027, 9, 23, 0, 0), utc(2027, 12, 21, 0, 0)};
    const char* names[4] = {"2027-03-20", "2027-06-21", "2027-09-23", "2027-12-21"};
    for (int d = 0; d < 4; ++d) {
        const uint32_t seed = kSeeds[d % 3];
        // From shortly before sunrise (about 02:00 UTC) to well after sunset (about 17:00 UTC).
        World w(seed, days[d] + 2 * 3600);
        w.calibrate();
        w.placePanel(0.0);
        double worst = 0;
        bool established = false;
        uint32_t establishedAt = 0;
        for (uint32_t step = 0; step < 15u * 3600u * 10u; ++step) {
            w.step();
            if (!w.sunUp()) continue;
            const double err = fabs(w.trackingError());
            if (!established && err < 5.0) {
                established = true;
                establishedAt = w.elapsedMs();
            }
            if (established && err > worst) worst = err;
        }
        char msg[96];
        snprintf(msg, sizeof(msg), "%s: worst error %.2f deg, %d moves, tracking established after %lu s", names[d], worst, w.powerOns(),
                 static_cast<unsigned long>(establishedAt / 1000));
        TEST_MESSAGE(msg);
        TEST_ASSERT_TRUE_MESSAGE(established, "never reached the sun target");
        TEST_ASSERT_TRUE_MESSAGE(worst < 6.0, msg);  // the proposal: threshold + 1 degree
        TEST_ASSERT_EQUAL(static_cast<int>(FaultCode::None), static_cast<int>(w.ctl().fault()));
        TEST_ASSERT_TRUE(w.powerOns() > 5 && w.powerOns() < 400);
    }
}

// 12 --------------------------------------------------------------------------- 72 hours unattended
static void scenario_12_soak_72h() {
    for (uint32_t seed : kSeeds) {
        World w(seed, utc(2027, 6, 21, 0, 0));
        w.calibrate();
        w.placePanel(0.0);
        sim::Rng wind(seed + 7);
        // Breeze with gusts, and two storms of ten minutes at hours 20 and 50.
        w.windKmh = [&wind](uint32_t ms) {
            const double h = ms / 3600000.0;
            double v = 9.0 + 5.0 * sin(h * 0.9) + 3.0 * wind.gauss();
            if ((h > 20.0 && h < 20.17) || (h > 50.0 && h < 50.17)) v = 46.0 + 3.0 * wind.gauss();
            return v < 0 ? 0.0 : v;
        };
        int faults = 0;
        const uint32_t total = 72u * 3600u * 10u;
        for (uint32_t step = 0; step < total; ++step) {
            if (step == 30u * 36000u) w.powerCut(10 * 60 * 1000);  // two short mains cuts
            if (step == 55u * 36000u) w.powerCut(10 * 60 * 1000);
            w.step();
            if (w.ctl().fault() != FaultCode::None) ++faults;
        }
        TEST_ASSERT_EQUAL_INT(0, faults);  // no false error in 72 h
        TEST_ASSERT_EQUAL_INT(0, w.relays().violations());
        TEST_ASSERT_EQUAL_INT(3, w.bootCount());
        int stows = 0;
        for (const auto& e : w.log().events())
            if (e.second.find("wind stow start") != std::string::npos) ++stows;
        TEST_ASSERT_TRUE_MESSAGE(stows >= 2, "both storms should have stowed the panel");
        // About 60 bytes per line and a 128 KB budget.
        char msg[80];
        snprintf(msg, sizeof(msg), "seed %lu: %u events in 72 h, %d moves", static_cast<unsigned long>(seed), static_cast<unsigned>(w.log().events().size()), w.powerOns());
        TEST_MESSAGE(msg);
        TEST_ASSERT_TRUE_MESSAGE(w.log().events().size() * 60 <= 128u * 1024u, msg);
    }
}

// 13 --------------------------------------------------------------------------- sudden reversal by hand
static void scenario_13_sudden_direction_change() {
    for (uint32_t seed : kSeeds) {
        World w(seed, utc(2027, 6, 21, 5, 0), settings::Mode::Manual);
        w.calibrate();
        w.placePanel(0.0);
        w.run(4000);
        w.ctl().command(Command::ManualEast, w.time().nowMs());
        for (int i = 0; i < 5; ++i) {
            w.run(1000);
            w.ctl().command(Command::KeepAlive, w.time().nowMs());
        }
        TEST_ASSERT_TRUE(w.relays().power());
        const uint32_t tCommand = w.time().nowMs();
        w.ctl().command(Command::ManualWest, w.time().nowMs());
        w.run(100);
        TEST_ASSERT_FALSE(w.relays().power());  // power is cut first
        uint32_t tReverse = 0;
        for (int i = 0; i < 300 && tReverse == 0; ++i) {  // find when power returns in the other direction
            w.run(100);
            w.ctl().command(Command::KeepAlive, w.time().nowMs());
            if (w.relays().power()) tReverse = w.time().nowMs();
        }
        TEST_ASSERT_NOT_EQUAL_UINT32(0, tReverse);
        TEST_ASSERT_TRUE(tReverse - tCommand >= 3000);  // at least the 3 s settle (the 10 s gap applies in fact)
        TEST_ASSERT_TRUE(tReverse - tCommand >= 10000);
        TEST_ASSERT_TRUE(w.relays().direction() == hal::Dir::West);
        TEST_ASSERT_EQUAL_INT(0, w.relays().violations());  // CH2 never changed under power
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(scenario_01_stall);
    RUN_TEST(scenario_02_wind);
    RUN_TEST(scenario_03_night);
    RUN_TEST(scenario_04_power_cut);
    RUN_TEST(scenario_05_angle_sensor_fault);
    RUN_TEST(scenario_06_relay_fault);
    RUN_TEST(scenario_07_end_of_stroke);
    RUN_TEST(scenario_08_wifi_off_does_not_matter);
    RUN_TEST(scenario_09_fault_reset);
    RUN_TEST(scenario_10_offline_update);
    RUN_TEST(scenario_11_tracking_accuracy);
    RUN_TEST(scenario_12_soak_72h);
    RUN_TEST(scenario_13_sudden_direction_change);
    return UNITY_END();
}
