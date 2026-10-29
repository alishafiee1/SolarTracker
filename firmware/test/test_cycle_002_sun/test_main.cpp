// Cycle 002 — sun position and target angle (rodo/suntracker/cycles/002-sun-position)
#include <math.h>
#include <stdio.h>
#include <unity.h>

#include "sun_position.h"
#include "tracking.h"
#include "../fixtures/spa_kerman.h"

void setUp() {}
void tearDown() {}

static double angleDiff(double a, double b) {
    double d = fmod(a - b + 540.0, 360.0) - 180.0;
    return fabs(d);
}

// Unix time of a UTC date, for readable tests (valid for 1970..2100).
static int64_t utc(int y, int m, int d, int hh, int mm) {
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int64_t days = 365LL * (y - 1970) + (y - 1969) / 4 + cum[m - 1] + (d - 1);
    if (m > 2 && (y % 4 == 0)) days += 1;
    return days * 86400 + hh * 3600 + mm * 60;
}

// R2: error <= 0.05 degrees against the NREL SPA reference over 2026-2035, all seasons, day and night.
static void accuracy_against_spa() {
    const size_t n = sizeof(kSpaKerman) / sizeof(kSpaKerman[0]);
    TEST_ASSERT_GREATER_OR_EQUAL(40, n);
    double worstAz = 0, worstZen = 0;
    for (size_t i = 0; i < n; ++i) {
        const SpaRow& r = kSpaKerman[i];
        const sun::SunPosition p = sun::position(r.utc, r.lat, r.lon);
        const double dz = fabs(p.zenithDeg - r.zenith);
        const double da = angleDiff(p.azimuthDeg, r.azimuth);
        if (dz > worstZen) worstZen = dz;
        if (da > worstAz) worstAz = da;
    }
    char msg[80];
    snprintf(msg, sizeof(msg), "worst azimuth error %.4f deg, worst zenith error %.4f deg", worstAz, worstZen);
    TEST_MESSAGE(msg);
    TEST_ASSERT_TRUE_MESSAGE(worstAz <= 0.05 && worstZen <= 0.05, msg);
}

// R3: known geometry.
static void rotation_formula_known_values() {
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -45.0, tracking::rotation({90.0, 45.0}));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 45.0, tracking::rotation({270.0, 45.0}));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, tracking::rotation({180.0, 30.0}));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -60.0, tracking::rotation({90.0, 60.0}));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, tracking::rotation({0.0, 0.0}));  // sun overhead: no division by zero
}

// Morning east (negative), noon near zero, afternoon west (positive), Kerman on 2027-06-21.
static void rotation_sign_over_the_day() {
    const double lat = 30.283, lon = 57.083;
    const double morning = tracking::rotation(sun::position(utc(2027, 6, 21, 3, 30), lat, lon));    // 07:00 local
    const double noon = tracking::rotation(sun::position(utc(2027, 6, 21, 8, 11), lat, lon));       // solar noon
    const double afternoon = tracking::rotation(sun::position(utc(2027, 6, 21, 13, 0), lat, lon));  // 16:30 local
    TEST_ASSERT_TRUE(morning < -20.0);
    TEST_ASSERT_TRUE(fabs(noon) < 7.0);
    TEST_ASSERT_TRUE(afternoon > 20.0);
}

// R4: clamping, default and custom range; sun lower than the range keeps the panel at the limit.
static void target_is_clamped() {
    TEST_ASSERT_EQUAL_DOUBLE(-45.0, tracking::target(-70.0));
    TEST_ASSERT_EQUAL_DOUBLE(45.0, tracking::target(70.0));
    TEST_ASSERT_EQUAL_DOUBLE(12.5, tracking::target(12.5));
    TEST_ASSERT_EQUAL_DOUBLE(-30.0, tracking::target(-44.0, -30.0, 30.0));
    TEST_ASSERT_EQUAL_DOUBLE(30.0, tracking::target(44.0, -30.0, 30.0));
    // Early morning in Kerman: sun far east and low, panel waits at the east limit.
    const double r = tracking::rotation(sun::position(utc(2027, 6, 21, 1, 40), 30.283, 57.083));  // 05:10 local
    TEST_ASSERT_EQUAL_DOUBLE(-45.0, tracking::target(r));
}

// R5: night flag matches the reference except within 0.05 degrees of the horizon.
static void night_flag_matches_reference() {
    const size_t n = sizeof(kSpaKerman) / sizeof(kSpaKerman[0]);
    size_t checked = 0, nights = 0;
    for (size_t i = 0; i < n; ++i) {
        const SpaRow& r = kSpaKerman[i];
        if (fabs(r.zenith - 90.0) < 0.05) continue;
        const bool night = sun::isNight(sun::position(r.utc, r.lat, r.lon));
        TEST_ASSERT_EQUAL_MESSAGE(r.zenith > 90.0, night, "night flag differs from SPA");
        ++checked;
        if (night) ++nights;
    }
    TEST_ASSERT_TRUE(checked > 100);
    TEST_ASSERT_TRUE(nights > 20);  // the table really contains night samples
}

// R5: sunrise and sunset found by scanning minutes lie within 1 minute of the SPA horizon crossings
// (12 dates across seasons and years, Kerman).
static void horizon_crossings_within_one_minute() {
    const size_t n = sizeof(kSpaHorizon) / sizeof(kSpaHorizon[0]);
    TEST_ASSERT_GREATER_OR_EQUAL(12, n);
    for (size_t i = 0; i < n; ++i) {
        const SpaHorizon& h = kSpaHorizon[i];
        int64_t rise = 0, set = 0;
        bool wasNight = true;
        for (int m = 0; m < 1440; ++m) {
            const int64_t t = h.dayStartUtc + m * 60;
            const bool night = sun::isNight(sun::position(t, 30.283, 57.083));
            if (wasNight && !night) rise = t;
            if (!wasNight && night) set = t;
            wasNight = night;
        }
        TEST_ASSERT_TRUE_MESSAGE(rise != 0 && set != 0, "no sunrise or sunset found in the day");
        TEST_ASSERT_INT64_WITHIN(60, h.sunriseUtc, rise);
        TEST_ASSERT_INT64_WITHIN(60, h.sunsetUtc, set);
    }
}

// Year boundary and leap day stay finite and in range.
static void azimuth_and_zenith_stay_in_range() {
    const int64_t times[] = {utc(2026, 12, 31, 23, 59), utc(2027, 1, 1, 0, 0), utc(2028, 2, 29, 12, 0), utc(2030, 12, 31, 12, 0)};
    for (int64_t t : times) {
        const sun::SunPosition p = sun::position(t, 30.283, 57.083);
        TEST_ASSERT_TRUE(p.azimuthDeg >= 0.0 && p.azimuthDeg < 360.0);
        TEST_ASSERT_TRUE(p.zenithDeg >= 0.0 && p.zenithDeg <= 180.0);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(accuracy_against_spa);
    RUN_TEST(rotation_formula_known_values);
    RUN_TEST(rotation_sign_over_the_day);
    RUN_TEST(target_is_clamped);
    RUN_TEST(night_flag_matches_reference);
    RUN_TEST(horizon_crossings_within_one_minute);
    RUN_TEST(azimuth_and_zenith_stay_in_range);
    return UNITY_END();
}
