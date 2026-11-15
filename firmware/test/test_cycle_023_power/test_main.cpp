// Cycle 023 — battery and mains watch (rodo/suntracker/cycles/023-battery-mains)
#include <unity.h>

#include "power_monitor.h"

using namespace power;

void setUp() {}
void tearDown() {}

static uint8_t run(PowerMonitor& m, int seconds, float batV, float chV, bool wired = true) {
    uint8_t all = 0;
    for (int i = 0; i < seconds; ++i) all |= m.onSecond(batV, chV, wired);
    return all;
}

static void percent_follows_the_cell_curve() {
    TEST_ASSERT_EQUAL_INT(100, percentFromVolts(4.25f));
    TEST_ASSERT_EQUAL_INT(0, percentFromVolts(3.2f));
    TEST_ASSERT_EQUAL_INT(48, percentFromVolts(3.80f));
    TEST_ASSERT_EQUAL_INT(55, percentFromVolts(3.85f));  // halfway between 48 and 62
    int prev = 101;
    for (float v = 4.3f; v > 3.2f; v -= 0.01f) {  // never goes up when the voltage goes down
        const int p = percentFromVolts(v);
        TEST_ASSERT_TRUE(p <= prev);
        prev = p;
    }
}

static void first_readings_are_silent_and_fill_the_info() {
    PowerMonitor m;
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 5, 3.95f, 5.0f));
    TEST_ASSERT_TRUE(m.info().batteryKnown);
    TEST_ASSERT_TRUE(m.info().mainsKnown);
    TEST_ASSERT_TRUE(m.info().mains);
    TEST_ASSERT_TRUE(m.info().state == BatteryState::Ok);
    TEST_ASSERT_INT_WITHIN(1, 70, m.info().percent);
}

static void mains_loss_and_return_are_reported_once_after_the_debounce() {
    PowerMonitor m;
    run(m, 3, 4.0f, 5.0f);
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 9, 4.0f, 0.0f));  // 9 s: not yet
    TEST_ASSERT_TRUE(m.info().mains);
    TEST_ASSERT_EQUAL_UINT8(kMainsLost, run(m, 1, 4.0f, 0.0f));
    TEST_ASSERT_FALSE(m.info().mains);
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 50, 4.0f, 0.0f));  // no repeat
    TEST_ASSERT_EQUAL_UINT8(kMainsRestored, run(m, 10, 4.0f, 5.0f));
    TEST_ASSERT_TRUE(m.info().mains);
}

static void a_short_dip_of_the_mains_is_ignored() {
    PowerMonitor m;
    run(m, 3, 4.0f, 5.0f);
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 5, 4.0f, 0.0f));
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 20, 4.0f, 5.0f));
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 5, 4.0f, 0.0f));
    TEST_ASSERT_TRUE(m.info().mains);
}

static void the_grey_zone_keeps_the_last_state() {
    PowerMonitor m;
    run(m, 3, 4.0f, 5.0f);
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 60, 4.0f, 3.5f));
    TEST_ASSERT_TRUE(m.info().mains);
}

static void starting_without_mains_is_not_a_loss_message() {
    PowerMonitor m;
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 30, 3.9f, 0.0f));
    TEST_ASSERT_FALSE(m.info().mains);
}

static void an_unwired_charger_pin_gives_no_mains_events() {
    PowerMonitor m;
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 60, 3.9f, 0.0f, false));
    TEST_ASSERT_FALSE(m.info().mainsKnown);
}

static void battery_low_critical_and_recovery_with_hysteresis() {
    PowerMonitor m;
    run(m, 5, 3.9f, 5.0f);
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 100, 3.65f, 5.0f));  // above the low limit: nothing
    // settle the smoothed value below the low limit, then the debounce
    const uint8_t low = run(m, 80, 3.5f, 5.0f);
    TEST_ASSERT_TRUE(low & kBatteryLow);
    TEST_ASSERT_TRUE(m.info().state == BatteryState::Low);
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 100, 3.5f, 5.0f));  // reported once
    const uint8_t crit = run(m, 80, 3.3f, 5.0f);
    TEST_ASSERT_TRUE(crit & kBatteryCritical);
    TEST_ASSERT_TRUE(m.info().state == BatteryState::Critical);
    TEST_ASSERT_EQUAL_UINT8(0, run(m, 80, 3.7f, 5.0f));  // 3.7 is below the 3.75 recovery limit
    TEST_ASSERT_TRUE(run(m, 80, 3.9f, 5.0f) & kBatteryRecovered);
    TEST_ASSERT_TRUE(m.info().state == BatteryState::Ok);
}

static void a_missing_battery_pin_reads_as_unknown() {
    PowerMonitor m;
    run(m, 5, 3.9f, 5.0f);
    run(m, 5, 0.05f, 5.0f);
    TEST_ASSERT_FALSE(m.info().batteryKnown);
    TEST_ASSERT_EQUAL_INT(-1, m.info().percent);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(percent_follows_the_cell_curve);
    RUN_TEST(first_readings_are_silent_and_fill_the_info);
    RUN_TEST(mains_loss_and_return_are_reported_once_after_the_debounce);
    RUN_TEST(a_short_dip_of_the_mains_is_ignored);
    RUN_TEST(the_grey_zone_keeps_the_last_state);
    RUN_TEST(starting_without_mains_is_not_a_loss_message);
    RUN_TEST(an_unwired_charger_pin_gives_no_mains_events);
    RUN_TEST(battery_low_critical_and_recovery_with_hysteresis);
    RUN_TEST(a_missing_battery_pin_reads_as_unknown);
    return UNITY_END();
}
