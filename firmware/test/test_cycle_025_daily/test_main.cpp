// Cycle 025 — one log line per day (rodo/suntracker/cycles/025-log-summary)
#include <string.h>
#include <unity.h>

#include "daily_summary.h"

void setUp() {}
void tearDown() {}

static void the_summary_comes_once_when_the_day_changes_and_counts_only_that_day() {
    events::DailySummary d;
    char line[96] = "";
    TEST_ASSERT_FALSE(d.onSecond(100, 5, 10000, 12.0f, 3.9f, 1.0f, line, sizeof(line)));  // the first day starts here
    TEST_ASSERT_FALSE(d.onSecond(100, 8, 25000, 55.0f, 3.7f, 5.5f, line, sizeof(line)));
    TEST_ASSERT_FALSE(d.onSecond(100, 9, 40000, 20.0f, 3.8f, 2.0f, line, sizeof(line)));
    TEST_ASSERT_TRUE(d.onSecond(101, 9, 40000, 3.0f, 3.9f, 0.5f, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("daily: moves=4 run_s=30 wind_max=55 bat_min=3.70 err_max=5.5", line);
    line[0] = '\0';
    TEST_ASSERT_FALSE(d.onSecond(101, 10, 41000, 3.0f, 3.9f, 0.5f, line, sizeof(line)));  // no second line the same day
    TEST_ASSERT_EQUAL_CHAR('\0', line[0]);
    TEST_ASSERT_TRUE(d.onSecond(102, 12, 47000, 1.0f, 3.9f, 0.1f, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("daily: moves=3 run_s=7 wind_max=3 bat_min=3.90 err_max=0.5", line);
}

static void a_missing_battery_and_no_tracking_error_are_shown_honestly() {
    events::DailySummary d;
    char line[96] = "";
    d.onSecond(7, 0, 0, 0.0f, 0.0f, -1.0f, line, sizeof(line));
    TEST_ASSERT_TRUE(d.onSecond(8, 0, 0, 0.0f, 0.0f, -1.0f, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("daily: moves=0 run_s=0 wind_max=0 bat_min=- err_max=0.0", line);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(the_summary_comes_once_when_the_day_changes_and_counts_only_that_day);
    RUN_TEST(a_missing_battery_and_no_tracking_error_are_shown_honestly);
    return UNITY_END();
}
