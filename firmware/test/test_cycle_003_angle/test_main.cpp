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

static void the_fault_cause_is_remembered() {  // cycle 033
    const Mount m{0, 0, 0};
    {
        sim::FakeAccel accel;
        AngleProvider p(accel);
        calibrate(p, accel, m);
        TEST_ASSERT_EQUAL_STRING("", p.faultCause());
        accel.setOk(false);
        for (int i = 0; i < 5; ++i) p.sample();
        TEST_ASSERT_FALSE(p.ok());
        TEST_ASSERT_EQUAL_STRING("i2c", p.faultCause());
    }
    {
        sim::FakeAccel accel;
        AngleProvider p(accel);
        calibrate(p, accel, m);
        feed(p, accel, reading(m, 0, 1.5f), 5);
        TEST_ASSERT_FALSE(p.ok());
        TEST_ASSERT_EQUAL_STRING("gravity", p.faultCause());
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.5f, p.lastBadMagnitude());
    }
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
}

// Cycle 019: a good reading beyond +-60 degrees is a warning, not a sensor fault; it clears by itself.
static void out_of_range_angle_is_a_warning_that_clears() {
    const Mount m{30, 40, 50};
    sim::FakeAccel accel;
    AngleProvider p(accel);
    calibrate(p, accel, m);
    feed(p, accel, reading(m, 10.0f), 8);
    TEST_ASSERT_FALSE(p.outOfRange());
    float live = 0.0f;
    TEST_ASSERT_TRUE(p.liveAngle(live));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 10.0f, live);

    feed(p, accel, reading(m, 75.0f), 4);  // 0.4 s: not yet
    TEST_ASSERT_FALSE(p.outOfRange());
    feed(p, accel, reading(m, 75.0f), 1);  // the fifth sample raises the warning
    TEST_ASSERT_TRUE(p.outOfRange());
    TEST_ASSERT_TRUE(p.ok());  // no fault, nothing latched
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 75.0f, p.angleDeg());  // the live angle is reported
    TEST_ASSERT_TRUE(p.liveAngle(live));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 75.0f, live);

    feed(p, accel, reading(m, 170.0f), 30);  // upside down for 3 s: still only a warning
    TEST_ASSERT_TRUE(p.outOfRange());
    TEST_ASSERT_TRUE(p.ok());

    feed(p, accel, reading(m, 20.0f), 4);
    TEST_ASSERT_TRUE(p.outOfRange());  // clears only after five good samples
    feed(p, accel, reading(m, 20.0f), 1);
    TEST_ASSERT_FALSE(p.outOfRange());
    feed(p, accel, reading(m, 20.0f), 5);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, p.angleDeg());  // the moving average starts again

    // A sensor that goes silent while out of range is still a fault.
    feed(p, accel, reading(m, 80.0f), 6);
    TEST_ASSERT_TRUE(p.outOfRange());
    feed(p, accel, {nanf(""), 0, 1}, 5);
    TEST_ASSERT_FALSE(p.ok());
    TEST_ASSERT_FALSE(p.liveAngle(live));
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


// ---- cycle 018: six-face offsets ----

// A cheap sensor: reading = true * gain + offset, per axis (the user's board reads Z +1.114 up and -0.906 down).
struct SensorError {
    Vec3 off;
    Vec3 gain;
};
static const SensorError kBoard = {{0.10f, -0.06f, 0.104f}, {1.01f, 0.99f, 1.01f}};

static Vec3 faulty(Vec3 g, SensorError e) { return {g.x * e.gain.x + e.off.x, g.y * e.gain.y + e.off.y, g.z * e.gain.z + e.off.z}; }

// Face f: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z up.
static Vec3 faceGravity(int f) {
    Vec3 v{0, 0, 0};
    const float s = (f % 2) ? -1.0f : 1.0f;
    if (f / 2 == 0) v.x = s;
    else if (f / 2 == 1) v.y = s;
    else v.z = s;
    return v;
}

static CalResult captureFace(AngleProvider& p, sim::FakeAccel& accel, Vec3 g) {
    TEST_ASSERT_TRUE(p.beginFaceCapture());
    feed(p, accel, g, static_cast<int>(angle::kCaptureSamples));
    return p.captureResult();
}

static void captureAllFaces(AngleProvider& p, sim::FakeAccel& accel, SensorError e) {
    const int order[6] = {4, 1, 2, 5, 0, 3};  // any order works
    for (int f : order) TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Ok), static_cast<int>(captureFace(p, accel, faulty(faceGravity(f), e))));
}

// R2, R3: the face is found from the reading itself; six faces give offset and gain per axis.
static void six_faces_give_offsets_and_gain() {
    sim::FakeAccel accel;
    sim::FakeSettingsStore store;
    AngleProvider p(accel);
    captureAllFaces(p, accel, kBoard);
    TEST_ASSERT_EQUAL_UINT8(0x3F, p.faceMask());
    TEST_ASSERT_TRUE(p.saveOffsets(store));
    TEST_ASSERT_FLOAT_WITHIN(0.005f, kBoard.off.x, p.offsets().offset[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, kBoard.off.y, p.offsets().offset[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, kBoard.off.z, p.offsets().offset[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, kBoard.gain.x, p.offsets().scale[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, kBoard.gain.y, p.offsets().scale[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, kBoard.gain.z, p.offsets().scale[2]);
}

// R1, R2: a few degrees off a face is still that face; between two faces is refused; a half set cannot be saved.
static void face_detection_and_rejections() {
    sim::FakeAccel accel;
    sim::FakeSettingsStore store;
    AngleProvider p(accel);
    // +X face, tilted 10 degrees: still face 0.
    TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Ok), static_cast<int>(captureFace(p, accel, rotZ({1, 0, 0}, 10.0f))));
    TEST_ASSERT_EQUAL_UINT8(1u << 0, p.faceMask());
    // 45 degrees between +Y and +Z: not a face.
    TEST_ASSERT_EQUAL(static_cast<int>(CalResult::BadFace), static_cast<int>(captureFace(p, accel, {0, 0.7071f, 0.7071f})));
    TEST_ASSERT_EQUAL_UINT8(1u << 0, p.faceMask());
    // Moving during the capture.
    TEST_ASSERT_TRUE(p.beginFaceCapture());
    for (int i = 0; i < static_cast<int>(angle::kCaptureSamples); ++i) {
        accel.setGravity(rotY({0, 0, 1}, (i % 2) ? 2.0f : -2.0f));
        p.sample();
    }
    TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Moving), static_cast<int>(p.captureResult()));
    // Unplugged during the capture.
    accel.setOk(false);
    TEST_ASSERT_TRUE(p.beginFaceCapture());
    feed(p, accel, {0, 0, 1}, 3);
    TEST_ASSERT_EQUAL(static_cast<int>(CalResult::SensorFault), static_cast<int>(p.captureResult()));
    accel.setOk(true);
    // Five faces are not enough.
    for (int f : {1, 2, 3, 4}) TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Ok), static_cast<int>(captureFace(p, accel, faceGravity(f))));
    TEST_ASSERT_EQUAL_UINT8(0x1F, p.faceMask());
    TEST_ASSERT_FALSE(p.saveOffsets(store));
}

// R3: capturing a face again replaces the earlier capture.
static void recapture_replaces_the_face() {
    sim::FakeAccel accel;
    sim::FakeSettingsStore store;
    AngleProvider p(accel);
    captureAllFaces(p, accel, kBoard);
    SensorError other = kBoard;
    other.off.z = 0.02f;
    // Redo both Z faces with another offset: the offset follows the newest captures.
    TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Ok), static_cast<int>(captureFace(p, accel, faulty(faceGravity(4), other))));
    TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Ok), static_cast<int>(captureFace(p, accel, faulty(faceGravity(5), other))));
    TEST_ASSERT_TRUE(p.saveOffsets(store));
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.02f, p.offsets().offset[2]);
}

// R4: before the offsets the board's Z-up reading (1.114 g) is refused; after them every mounting works and the angle is exact.
static void offsets_make_every_orientation_calibratable() {
    sim::FakeAccel accel;
    sim::FakeSettingsStore store;
    AngleProvider p(accel);
    // Flat, Z up: |g| = 1.114 > 1.1.
    TEST_ASSERT_EQUAL(static_cast<int>(CalResult::BadGravity), static_cast<int>(capture(p, accel, CalStep::Horizontal, faulty(reading({0, 0, 0}, 0), kBoard))));
    captureAllFaces(p, accel, kBoard);
    TEST_ASSERT_TRUE(p.saveOffsets(store));
    for (const Mount& m : kMounts) {
        TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Ok), static_cast<int>(capture(p, accel, CalStep::Horizontal, faulty(reading(m, 0), kBoard))));
        TEST_ASSERT_EQUAL(static_cast<int>(CalResult::Ok), static_cast<int>(capture(p, accel, CalStep::East, faulty(reading(m, -8.0f), kBoard))));
        for (int phi = -45; phi <= 45; phi += 15) {
            feed(p, accel, faulty(reading(m, static_cast<float>(phi)), kBoard), 5);
            TEST_ASSERT_FLOAT_WITHIN(0.5f, static_cast<float>(phi), p.angleDeg());
        }
        TEST_ASSERT_TRUE(p.ok());
    }
}

// R5, R6: offsets persist; saving them drops the angle calibration (also after a restart); reset clears them.
static void offsets_persist_and_drop_the_angle_calibration() {
    sim::FakeSettingsStore store;
    sim::FakeAccel accel;
    AngleProvider p(accel);
    calibrate(p, accel, {0, 0, 0});
    TEST_ASSERT_TRUE(p.saveTo(store));
    captureAllFaces(p, accel, kBoard);
    TEST_ASSERT_TRUE(p.saveOffsets(store));
    TEST_ASSERT_FALSE(p.calibrated());

    sim::FakeAccel accel2;
    AngleProvider q(accel2);
    TEST_ASSERT_FALSE(q.loadFrom(store));  // the old angle calibration is gone ...
    TEST_ASSERT_TRUE(q.offsetsSaved());     // ... but the offsets are back
    feed(q, accel2, faulty({0, 0, 1}, kBoard), 3);
    const hal::Vec3 g = q.lastGravity();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, sqrtf(g.x * g.x + g.y * g.y + g.z * g.z));

    TEST_ASSERT_TRUE(q.resetOffsets(store));
    TEST_ASSERT_FALSE(q.offsetsSaved());
    TEST_ASSERT_EQUAL_UINT8(0, q.faceMask());
    sim::FakeAccel accel3;
    AngleProvider r(accel3);
    TEST_ASSERT_FALSE(r.loadFrom(store));
    TEST_ASSERT_FALSE(r.offsetsSaved());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(tracks_angle_in_any_mounting);
    RUN_TEST(east_is_negative);
    RUN_TEST(rejects_bad_calibrations);
    RUN_TEST(moving_average_smooths_noise);
    RUN_TEST(fault_within_one_second_when_unplugged);
    RUN_TEST(the_fault_cause_is_remembered);
    RUN_TEST(single_bad_sample_is_tolerated);
    RUN_TEST(fault_on_nan_and_impossible_gravity_and_implausible_angle);
    RUN_TEST(out_of_range_angle_is_a_warning_that_clears);
    RUN_TEST(fault_is_latched_until_cleared);
    RUN_TEST(not_calibrated_by_default);
    RUN_TEST(calibration_persists);
    RUN_TEST(six_faces_give_offsets_and_gain);
    RUN_TEST(face_detection_and_rejections);
    RUN_TEST(recapture_replaces_the_face);
    RUN_TEST(offsets_make_every_orientation_calibratable);
    RUN_TEST(offsets_persist_and_drop_the_angle_calibration);
    return UNITY_END();
}
