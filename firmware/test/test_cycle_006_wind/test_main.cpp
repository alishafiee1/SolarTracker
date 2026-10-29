// Cycle 006 — high-wind protection (rodo/suntracker/cycles/006-wind-protection)
#include <math.h>
#include <unity.h>

#include "wind_monitor.h"

using wind::Config;
using wind::WindMonitor;

void setUp() {}
void tearDown() {}

// Simulated anemometer: one pulse per revolution at a rate that matches the wanted speed.
struct Rig {
    WindMonitor mon;
    Config cfg;
    uint32_t nowMs = 0;
    uint32_t nowUs;
    double nextPulseUs;  // time of the next pulse in a 64-bit local clock
    double clockUs = 0;
    bool started = false;
    bool ended = false;
    float peakAtEnd = 0;
    int startCount = 0;

    explicit Rig(uint32_t startUs = 1000000) : nowUs(startUs), nextPulseUs(0) { clockUs = startUs; nextPulseUs = clockUs; }

    // Run for `ms` at a constant wind speed (0 = calm: no pulses at all).
    void run(uint32_t ms, float kmh) {
        for (uint32_t t = 0; t < ms; t += 100) {
            nowMs += 100;
            clockUs += 100000.0;
            nowUs = static_cast<uint32_t>(clockUs);  // wraps like micros()
            if (kmh > 0.0f) {
                const double periodUs = 1.0e6 / (kmh / cfg.factorKmhPerHz);
                if (nextPulseUs < clockUs - 4.0 * periodUs) nextPulseUs = clockUs;  // after a calm spell
                while (nextPulseUs <= clockUs) {
                    mon.feed(static_cast<uint32_t>(nextPulseUs));
                    nextPulseUs += periodUs;
                }
            }
            const wind::Result r = mon.update(nowMs, nowUs, cfg);
            if (r.stowStarted) {
                started = true;
                ++startCount;
            }
            if (r.stowEnded) {
                ended = true;
                peakAtEnd = r.peakKmh;
            }
        }
    }
};

// ------------------------------------------------------------ R1 / R2
static void speed_follows_the_pulse_rate() {
    Rig r;
    r.run(6000, 40.8f);  // 6 Hz at the default factor of 6.8 km/h per Hz
    TEST_ASSERT_FLOAT_WITHIN(0.6f, 40.8f, r.mon.speedKmh());
    r.run(6000, 13.6f);  // 2 Hz
    TEST_ASSERT_FLOAT_WITHIN(0.4f, 13.6f, r.mon.speedKmh());
    r.run(6000, 0.0f);   // calm
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.mon.speedKmh());
}

static void the_factor_is_applied() {
    Rig r;
    r.cfg.factorKmhPerHz = 3.4f;
    r.run(6000, 20.4f);  // 6 Hz again
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.4f, r.mon.speedKmh());
}

static void noise_pulses_closer_than_5_ms_are_ignored() {
    Rig r;
    r.run(4000, 27.2f);  // 4 Hz
    const float clean = r.mon.speedKmh();
    // Contact bounce 1 ms after the real pulses must not change the result.
    WindMonitor m;
    uint32_t t = 100000;
    for (int i = 0; i < 20; ++i) {
        m.feed(t);
        m.feed(t + 1000);  // bounce
        t += 250000;       // 4 Hz
    }
    m.update(0, t, Config());
    TEST_ASSERT_FLOAT_WITHIN(0.6f, clean, m.speedKmh());
}

static void works_across_the_microsecond_clock_wrap() {
    Rig r(0xFFF00000u);  // micros() wraps about one second into the run
    r.run(8000, 40.8f);
    TEST_ASSERT_FLOAT_WITHIN(0.8f, 40.8f, r.mon.speedKmh());
}

static void a_stopped_rotor_reads_zero_after_three_seconds() {
    Rig r;
    r.run(5000, 40.8f);
    r.run(3500, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.mon.speedKmh());
}

// ------------------------------------------------------------ R3 (feature scenarios)
static void sustained_strong_wind_starts_the_stow_after_five_seconds() {
    Rig r;
    r.run(2000, 10.0f);
    r.run(4800, 45.0f);  // not yet 5 s above the limit
    TEST_ASSERT_FALSE(r.mon.stow());
    r.run(3200, 45.0f);  // 8 s in total
    TEST_ASSERT_TRUE(r.mon.stow());
    TEST_ASSERT_TRUE(r.started);
}

static void a_short_gust_is_ignored() {
    Rig r;
    r.run(4000, 50.0f);
    r.run(6000, 20.0f);
    TEST_ASSERT_FALSE(r.mon.stow());
    TEST_ASSERT_FALSE(r.started);
}

static void exactly_the_limit_is_not_strong_wind() {
    Rig r;
    r.cfg.factorKmhPerHz = 4.0f;  // 10 Hz = a pulse every 100 ms exactly, so the speed is exactly 40.0
    r.run(20000, 40.0f);
    TEST_ASSERT_EQUAL_FLOAT(40.0f, r.mon.speedKmh());
    TEST_ASSERT_FALSE(r.mon.stow());  // not strictly above the 40 km/h limit
    r.run(8000, 41.2f);
    TEST_ASSERT_TRUE(r.mon.stow());
}

// ------------------------------------------------------------ R4
static void tracking_returns_only_after_fifteen_calm_minutes() {
    Rig r;
    r.run(8000, 45.0f);
    TEST_ASSERT_TRUE(r.mon.stow());
    r.run(14 * 60 * 1000 + 50 * 1000, 20.0f);  // 14 min 50 s calm
    TEST_ASSERT_TRUE(r.mon.stow());
    TEST_ASSERT_FALSE(r.ended);
    r.run(30 * 1000, 20.0f);
    TEST_ASSERT_FALSE(r.mon.stow());
    TEST_ASSERT_TRUE(r.ended);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 45.0f, r.peakAtEnd);
}

static void a_gust_during_the_wait_restarts_the_fifteen_minutes() {
    Rig r;
    r.run(8000, 45.0f);
    r.run(10 * 60 * 1000, 20.0f);       // 10 minutes calm
    r.run(3000, 35.0f);                  // a 35 km/h gust: above the return speed, below the stow speed
    r.run(14 * 60 * 1000 + 50 * 1000, 20.0f);  // 14 min 50 s calm again
    TEST_ASSERT_TRUE(r.mon.stow());      // still stowed: the timer started over
    r.run(30 * 1000, 20.0f);
    TEST_ASSERT_FALSE(r.mon.stow());
}

static void remaining_time_is_reported() {
    Rig r;
    r.run(8000, 45.0f);
    TEST_ASSERT_EQUAL_UINT32(900, r.mon.returnRemainingS(r.nowMs, r.cfg));
    r.run(5 * 60 * 1000, 20.0f);
    TEST_ASSERT_UINT32_WITHIN(3, 600, r.mon.returnRemainingS(r.nowMs, r.cfg));
    Rig calm;
    TEST_ASSERT_EQUAL_UINT32(0, calm.mon.returnRemainingS(0, calm.cfg));
}

static void changed_thresholds_apply_at_once() {
    Rig r;
    r.cfg.stowKmh = 20.0f;
    r.cfg.returnKmh = 10.0f;
    r.cfg.stowDelayS = 2;
    r.cfg.returnMin = 1;
    r.run(4000, 27.2f);
    TEST_ASSERT_TRUE(r.mon.stow());
    r.run(70 * 1000, 5.0f);
    TEST_ASSERT_FALSE(r.mon.stow());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(speed_follows_the_pulse_rate);
    RUN_TEST(the_factor_is_applied);
    RUN_TEST(noise_pulses_closer_than_5_ms_are_ignored);
    RUN_TEST(works_across_the_microsecond_clock_wrap);
    RUN_TEST(a_stopped_rotor_reads_zero_after_three_seconds);
    RUN_TEST(sustained_strong_wind_starts_the_stow_after_five_seconds);
    RUN_TEST(a_short_gust_is_ignored);
    RUN_TEST(exactly_the_limit_is_not_strong_wind);
    RUN_TEST(tracking_returns_only_after_fifteen_calm_minutes);
    RUN_TEST(a_gust_during_the_wait_restarts_the_fifteen_minutes);
    RUN_TEST(remaining_time_is_reported);
    RUN_TEST(changed_thresholds_apply_at_once);
    return UNITY_END();
}
