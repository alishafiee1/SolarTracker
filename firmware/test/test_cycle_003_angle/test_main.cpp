// Cycle 003 — panel angle sensing and calibration (rodo/suntracker/cycles/003-panel-angle)
#include <fakes.h>
#include <math.h>
#include <unity.h>

#include "angle_provider.h"

using angle::AngleProvider;
using angle::CalResult;
using angle::CalStep;
using hal::Vec3;

void setUp() {}
void tearDown() {}

constexpr float kRad = 3.14159265358979f / 180.0f;

static Vec3 rotX(Vec3 v, float deg) {
    const float c = cosf(deg * kRad), s = sinf(deg * kRad);
    return {v.x, c * v.y - s * v.z, s * v.y + c * v.z};
}
static Vec3 rotY(Vec3 v, float deg) {
    const float c = cosf(deg * kRad), s = sinf(deg * kRad);
    return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}
static Vec3 rotZ(Vec3 v, float deg) {
    const float c = cosf(deg * kRad), s = sinf(deg * kRad);
    return {c * v.x - s * v.y, s * v.x + c * v.y, v.z};
}

// A box mounted under the panel in an arbitrary orientation (three Euler angles).
struct Mount {
    float a, b, c;
};

// What the sensor reads when the panel is rotated by phiDeg (positive = west) and the box is mounted as given.
static Vec3 reading(Mount m, float phiDeg, float magnitude = 1.0f) {
    Vec3 v = rotX({0, 0, magnitude}, -phiDeg);  // gravity as seen from the rotated panel
    return rotZ(rotY(rotX(v, m.a), m.b), m.c);  // then through the fixed mounting rotation
}

static void feed(AngleProvider& p, sim::FakeAccel& accel, Vec3 g, int n) {
    accel.setGravity(g);
    for (int i = 0; i < n; ++i) p.sample();
}

static CalResult capture(AngleProvider& p, sim::FakeAccel& accel, CalStep step, Vec3 g) {
    TEST_ASSERT_TRUE(p.beginCapture(step));
    feed(p, accel, g, static_cast<int>(angle::kCaptureSamples));
    return p.captureResult();
}

static void calibrate(AngleProvider& p, sim::FakeAccel& accel, Mount m, float eastDeg = -8.0f) {
    TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Ok), static_cast<int>(capture(p, accel, CalStep::Horizontal, reading(m, 0))));
    TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Ok), static_cast<int>(capture(p, accel, CalStep::East, reading(m, eastDeg))));
    TEST_ASSERT_TRUE(p.calibrated());
}

static const Mount kMounts[] = {
    {0, 0, 0},       {90, 0, 0},   {0, 90, 0},     {0, 180, 0},  // upside down
    {30, 40, 50},    {123, -67, 200}, {-45, 15, 300}, {270, 270, 10},
};

// R2: any mounting orientation, angle within 0.5 degrees over -45..+45.
static void tracks_angle_in_any_mounting() {
    for (const Mount& m : kMounts) {
        sim::FakeAccel accel;
        AngleProvider p(accel);
        calibrate(p, accel, m);
        for (int phi = -45; phi <= 45; phi += 5) {
            feed(p, accel, reading(m, static_cast<float>(phi)), 5);
            TEST_ASSERT_FLOAT_WITHIN(0.5f, static_cast<float>(phi), p.angleDeg());
        }
        TEST_ASSERT_TRUE(p.ok());
    }
}

// East is negative: the position used as the east calibration step reads negative.
static void east_is_negative() {
    sim::FakeAccel accel;
    AngleProvider p(accel);
    const Mount m{30, 40, 50};
    calibrate(p, accel, m, -10.0f);
    feed(p, accel, reading(m, -10.0f), 5);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, -10.0f, p.angleDeg());
    feed(p, accel, reading(m, 20.0f), 5);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 20.0f, p.angleDeg());
}

// R3: invalid calibrations are rejected and leave the system uncalibrated.
static void rejects_bad_calibrations() {
    const Mount m{10, 20, 30};
    {
        sim::FakeAccel accel;
        AngleProvider p(accel);
        TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Ok), static_cast<int>(capture(p, accel, CalStep::Horizontal, reading(m, 0))));
        TEST_ASSERT_EQUAL(static_cast<int>(CalResult::TooSmallAngle), static_cast<int>(capture(p, accel, CalStep::East, reading(m, -2))));
        TEST_ASSERT_FALSE(p.calibrated());
    }
    {
        sim::FakeAccel accel;
        AngleProvider p(accel);
        capture(p, accel, CalStep::Horizontal, reading(m, 0));
        TEST_ASSERT_EQUAL(static_cast<int>(CalResult::TooLargeAngle), static_cast<int>(capture(p, accel, CalStep::East, reading(m, -35))));
        TEST_ASSERT_FALSE(p.calibrated());
    }
    {  // panel still moving: samples alternate by 1 degree
        sim::FakeAccel accel;
        AngleProvider p(accel);
        TEST_ASSERT_TRUE(p.beginCapture(CalStep::Horizontal));
        for (size_t i = 0; i < angle::kCaptureSamples; ++i) {
            accel.setGravity(reading(m, (i % 2) ? -1.0f : 0.0f));
            p.sample();
        }
        TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Moving), static_cast<int>(p.captureResult()));
    }
    {  // gravity magnitude far from 1 g (still a plausible sample, but the mean is not)
        sim::FakeAccel accel;
        AngleProvider p(accel);
        TEST_ASSERT_EQUAL(static_cast<int>(CalResult::BadGravity), static_cast<int>(capture(p, accel, CalStep::Horizontal, reading(m, 0, 0.85f))));
    }
    {  // the east step cannot come first
        sim::FakeAccel accel;
        AngleProvider p(accel);
        TEST_ASSERT_FALSE(p.beginCapture(CalStep::East));
    }
    {  // sensor dies in the middle of a capture
        sim::FakeAccel accel;
        AngleProvider p(accel);
        TEST_ASSERT_TRUE(p.beginCapture(CalStep::Horizontal));
        feed(p, accel, reading(m, 0), 10);
        accel.setOk(false);
        p.sample();
        TEST_ASSERT_EQUAL(static_cast<int>(CalResult::SensorFault), static_cast<int>(p.captureResult()));
        TEST_ASSERT_FALSE(p.capturing());
    }
}

// R4: the moving average smooths vibration (alternating +-1 degree leaves at most 0.2 degrees of error).
static void moving_average_smooths_noise() {
    sim::FakeAccel accel;
    AngleProvider p(accel);
    const Mount m{0, 0, 0};
    calibrate(p, accel, m);
    for (int i = 0; i < 10; ++i) {
        accel.setGravity(reading(m, 20.0f + ((i % 2) ? 1.0f : -1.0f)));
        p.sample();
    }
    TEST_ASSERT_FLOAT_WITHIN(0.25f, 20.0f, p.angleDeg());
}

// R5: a sensor fault is reported within 1 s (5 samples at 10 Hz); single bad samples are tolerated.
static void fault_within_one_second_when_unplugged() {
    sim::FakeAccel accel;
    AngleProvider p(accel);
    const Mount m{0, 0, 0};
    calibrate(p, accel, m);
    feed(p, accel, reading(m, 10), 5);
    accel.setOk(false);
    for (int i = 0; i < 4; ++i) p.sample();
    TEST_ASSERT_TRUE(p.ok());
    p.sample();  // 5th consecutive failed read = 0.5 s
    TEST_ASSERT_FALSE(p.ok());
}

static void single_bad_sample_is_tolerated() {
    sim::FakeAccel accel;
    AngleProvider p(accel);
    const Mount m{0, 0, 0};
    calibrate(p, accel, m);
    feed(p, accel, reading(m, 10), 5);
    accel.setOk(false);
    p.sample();
    accel.setOk(true);
    feed(p, accel, reading(m, 10), 20);
    TEST_ASSERT_TRUE(p.ok());
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 10.0f, p.angleDeg());
}

static void fault_on_nan_and_impossible_gravity_and_implausible_angle() {
    const Mount m{0, 0, 0};
    const float nan = nanf("");
    {
        sim::FakeAccel accel;
        AngleProvider p(accel);
        calibrate(p, accel, m);
        feed(p, accel, {nan, 0, 1}, 5);
        TEST_ASSERT_FALSE(p.ok());
    }
    {
        sim::FakeAccel accel;
        AngleProvider p(accel);
        calibrate(p, accel, m);
        feed(p, accel, reading(m, 0, 1.5f), 5);  // 1.5 g: free fall or a shock cannot last 0.5 s
        TEST_ASSERT_FALSE(p.ok());
    }
    {
        sim::FakeAccel accel;
        AngleProvider p(accel);
        calibrate(p, accel, m);
        feed(p, accel, reading(m, 75.0f), 5);  // beyond +-60 degrees is not a position of this tracker
        TEST_ASSERT_FALSE(p.ok());
    }
}

// The fault is latched until cleared; after that tracking works again.
static void fault_is_latched_until_cleared() {
    sim::FakeAccel accel;
    AngleProvider p(accel);
    const Mount m{0, 0, 0};
    calibrate(p, accel, m);
    accel.setOk(false);
    for (int i = 0; i < 5; ++i) p.sample();
    TEST_ASSERT_FALSE(p.ok());
    accel.setOk(true);
    feed(p, accel, reading(m, 15), 10);
    TEST_ASSERT_FALSE(p.ok());
    p.clearFault();
    feed(p, accel, reading(m, 15), 5);
    TEST_ASSERT_TRUE(p.ok());
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 15.0f, p.angleDeg());
}

// R6: without a valid calibration the provider says so (the state machine then refuses automatic moves).
static void not_calibrated_by_default() {
    sim::FakeAccel accel;
    AngleProvider p(accel);
    TEST_ASSERT_FALSE(p.calibrated());
    feed(p, accel, reading({0, 0, 0}, 10), 10);
    TEST_ASSERT_FALSE(p.calibrated());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, p.angleDeg());
    // Starting a new horizontal capture also drops an existing calibration.
    calibrate(p, accel, {0, 0, 0});
    TEST_ASSERT_TRUE(p.beginCapture(CalStep::Horizontal));
    TEST_ASSERT_FALSE(p.calibrated());
}

// R7: the calibration survives a restart through the settings store; garbage is not accepted.
static void calibration_persists() {
    sim::FakeSettingsStore store;
    const Mount m{30, 40, 50};
    {
        sim::FakeAccel accel;
        AngleProvider p(accel);
        TEST_ASSERT_FALSE(p.saveTo(store));  // nothing to save yet
        calibrate(p, accel, m);
        TEST_ASSERT_TRUE(p.saveTo(store));
    }
    {
        sim::FakeAccel accel;
        AngleProvider p(accel);
        TEST_ASSERT_TRUE(p.loadFrom(store));
        TEST_ASSERT_TRUE(p.calibrated());
        feed(p, accel, reading(m, -25.0f), 5);
        TEST_ASSERT_FLOAT_WITHIN(0.5f, -25.0f, p.angleDeg());
    }
    {
        sim::FakeSettingsStore bad;
        angle::AngleCalibration junk{{nanf(""), 0, 0}, {0, 0, 1}, 0};
        bad.save("angle_cal", &junk, sizeof(junk));
        sim::FakeAccel accel;
        AngleProvider p(accel);
        TEST_ASSERT_FALSE(p.loadFrom(bad));
        TEST_ASSERT_FALSE(p.calibrated());
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(tracks_angle_in_any_mounting);
    RUN_TEST(east_is_negative);
    RUN_TEST(rejects_bad_calibrations);
    RUN_TEST(moving_average_smooths_noise);
    RUN_TEST(fault_within_one_second_when_unplugged);
    RUN_TEST(single_bad_sample_is_tolerated);
    RUN_TEST(fault_on_nan_and_impossible_gravity_and_implausible_angle);
    RUN_TEST(fault_is_latched_until_cleared);
    RUN_TEST(not_calibrated_by_default);
    RUN_TEST(calibration_persists);
    return UNITY_END();
}
