// Cycle 001 — firmware skeleton (rodo/suntracker/cycles/001-firmware-skeleton)
#include <fakes.h>
#include <string.h>
#include <unity.h>

#include "boot_info.h"
#include "control_loop.h"
#include "tick_scheduler.h"

void setUp() {}
void tearDown() {}

// Scenario @R6: the control loop runs every 100 ms.
static void control_loop_ticks_ten_times_per_second() {
    sim::FakeTime time;
    core::ControlLoop loop;
    core::TickScheduler sched(time.nowMs(), core::kControlPeriodMs);
    for (int ms = 0; ms < 1000; ++ms) {
        time.advance(1);
        if (sched.due(time.nowMs())) loop.tick(time.nowMs());
    }
    TEST_ASSERT_EQUAL_UINT32(10, loop.ticks());
    TEST_ASSERT_EQUAL_UINT32(1000, loop.lastTickMs());
}

static void scheduler_survives_millis_wraparound() {
    const uint32_t start = 0xFFFFFF00u;
    core::TickScheduler sched(start, 100);
    int ticks = 0;
    for (uint32_t t = start; t != start + 1000u; ++t) {
        if (sched.due(t + 1)) ++ticks;
    }
    TEST_ASSERT_EQUAL_INT(10, ticks);
}

static void scheduler_does_not_burst_after_a_stall() {
    core::TickScheduler sched(0, 100);
    TEST_ASSERT_TRUE(sched.due(100));
    TEST_ASSERT_TRUE(sched.due(750));   // loop was blocked for 650 ms
    TEST_ASSERT_FALSE(sched.due(751));  // no catch-up burst
    TEST_ASSERT_FALSE(sched.due(849));
    TEST_ASSERT_TRUE(sched.due(850));
}

// R4 in the simulator: relays start inactive, like the board after power-up.
static void relays_start_off() {
    sim::FakeRelays relays;
    TEST_ASSERT_FALSE(relays.power());
    TEST_ASSERT_EQUAL_size_t(0, relays.writes().size());
}

// Hardware interlock model: CH2 never switches while CH1 is on.
static void relays_refuse_direction_change_under_power() {
    sim::FakeTime time;
    sim::FakeRelays relays(&time);
    TEST_ASSERT_TRUE(relays.setDirection(hal::Dir::West));
    relays.setPower(true);
    TEST_ASSERT_FALSE(relays.setDirection(hal::Dir::East));
    TEST_ASSERT_EQUAL_INT(1, relays.violations());
    TEST_ASSERT_TRUE(relays.direction() == hal::Dir::West);
}

// R5: reset reasons have stable names for the serial log and event log.
static void reset_reason_names() {
    TEST_ASSERT_EQUAL_STRING("power-on", core::resetReasonName(core::ResetReason::PowerOn));
    TEST_ASSERT_EQUAL_STRING("watchdog", core::resetReasonName(core::ResetReason::Watchdog));
    TEST_ASSERT_EQUAL_STRING("brownout", core::resetReasonName(core::ResetReason::Brownout));
    TEST_ASSERT_EQUAL_STRING("unknown", core::resetReasonName(core::ResetReason::Unknown));
}

// R7: boot banner format.
static void boot_banner_format() {
    char buf[96];
    core::formatBootBanner(buf, sizeof(buf), "0.1.0", core::ResetReason::Watchdog);
    TEST_ASSERT_EQUAL_STRING("SunTracker 0.1.0 \xE2\x80\x94 reset: watchdog", buf);
}

static void boot_banner_truncates_safely() {
    char buf[12];
    memset(buf, 'x', sizeof(buf));
    core::formatBootBanner(buf, sizeof(buf), "0.1.0", core::ResetReason::PowerOn);
    TEST_ASSERT_EQUAL_CHAR('\0', buf[sizeof(buf) - 1]);
}

static void fake_settings_store_round_trip() {
    sim::FakeSettingsStore store;
    const uint32_t value = 0xC0FFEEu;
    uint32_t out = 0;
    TEST_ASSERT_FALSE(store.load("k", &out, sizeof(out)));
    TEST_ASSERT_TRUE(store.save("k", &value, sizeof(value)));
    TEST_ASSERT_TRUE(store.load("k", &out, sizeof(out)));
    TEST_ASSERT_EQUAL_HEX32(value, out);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(control_loop_ticks_ten_times_per_second);
    RUN_TEST(scheduler_survives_millis_wraparound);
    RUN_TEST(scheduler_does_not_burst_after_a_stall);
    RUN_TEST(relays_start_off);
    RUN_TEST(relays_refuse_direction_change_under_power);
    RUN_TEST(reset_reason_names);
    RUN_TEST(boot_banner_format);
    RUN_TEST(boot_banner_truncates_safely);
    RUN_TEST(fake_settings_store_round_trip);
    return UNITY_END();
}
