#include "angle_provider.h"

#include <math.h>
#include <string.h>

namespace angle {

namespace {

constexpr float kRad = 3.14159265358979f / 180.0f;
constexpr float kMovingLimitDeg = 0.3f;   // R3: samples may vary at most this much during a capture
constexpr float kMinStepDeg = 3.0f;       // R3
constexpr float kMaxStepDeg = 30.0f;      // R3
constexpr float kImplausibleDeg = 60.0f;  // beyond this the reading is treated as a sensor fault
constexpr const char* kStoreKey = "angle_cal";

using hal::Vec3;

Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float norm(Vec3 a) { return sqrtf(dot(a, a)); }
Vec3 unit(Vec3 a) {
    const float n = norm(a);
    return n > 0.0f ? scale(a, 1.0f / n) : Vec3{0, 0, 0};
}
float clamp1(float x) { return x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x); }
float angleBetweenDeg(Vec3 a, Vec3 b) { return acosf(clamp1(dot(unit(a), unit(b)))) / kRad; }

}  // namespace

bool AngleProvider::validGravity(const Vec3& g, float lo, float hi) {
    if (!isfinite(g.x) || !isfinite(g.y) || !isfinite(g.z)) return false;
    const float n = norm(g);
    return n >= lo && n <= hi;
}

bool AngleProvider::angleOf(const Vec3& g, float& deg) const {
    if (!calibrated_) return false;
    // Project gravity onto the rotation plane and measure from the horizontal reference.
    const Vec3 gp = sub(g, scale(axis_, dot(g, axis_)));
    if (norm(gp) < 1e-3f) return false;
    // East (the g1 side) is negative, so flip the sign of the right-handed angle about the axis.
    deg = -atan2f(dot(cross(ref_, gp), axis_), dot(ref_, gp)) / kRad;
    return true;
}

void AngleProvider::pushAngle(float deg) {
    window_[windowPos_] = deg;
    windowPos_ = (windowPos_ + 1) % kAverageSamples;
    if (windowCount_ < kAverageSamples) ++windowCount_;
}

void AngleProvider::sample() {
    Vec3 g{0, 0, 0};
    bool valid = accel_.read(g) && validGravity(g, 0.8f, 1.2f);

    if (capturing_) {
        if (!valid) {
            capturing_ = false;
            result_ = CalResult::SensorFault;
            return;
        }
        samples_[sampleCount_++] = g;
        if (sampleCount_ == kCaptureSamples) finishCapture();
        return;  // the angle is not tracked while calibrating
    }

    float deg = 0.0f;
    if (valid && calibrated_) {
        valid = angleOf(g, deg) && fabsf(deg) <= kImplausibleDeg;
    }

    if (!valid) {
        if (invalidRun_ < 255) ++invalidRun_;
        if (invalidRun_ >= kFaultAfterInvalid) {
            fault_ = true;
            windowCount_ = 0;
            windowPos_ = 0;
        }
        return;
    }
    invalidRun_ = 0;
    if (fault_ || !calibrated_) return;
    pushAngle(deg);
}

float AngleProvider::angleDeg() const {
    if (!calibrated_ || windowCount_ == 0) return 0.0f;
    float sum = 0.0f;
    for (size_t i = 0; i < windowCount_; ++i) sum += window_[i];
    return sum / static_cast<float>(windowCount_);
}

void AngleProvider::clearFault() {
    fault_ = false;
    invalidRun_ = 0;
    windowCount_ = 0;
    windowPos_ = 0;
}

bool AngleProvider::beginCapture(CalStep step) {
    if (step == CalStep::East && !horizontalDone_) return false;
    capturing_ = true;
    step_ = step;
    result_ = CalResult::Pending;
    sampleCount_ = 0;
    if (step == CalStep::Horizontal) {
        // A new horizontal capture starts a new calibration.
        horizontalDone_ = false;
        calibrated_ = false;
        windowCount_ = 0;
        windowPos_ = 0;
    }
    return true;
}

void AngleProvider::cancelCapture() {
    capturing_ = false;
    result_ = CalResult::NotCaptured;
}

void AngleProvider::finishCapture() {
    capturing_ = false;

    Vec3 sum{0, 0, 0};
    for (size_t i = 0; i < kCaptureSamples; ++i) sum = {sum.x + samples_[i].x, sum.y + samples_[i].y, sum.z + samples_[i].z};
    const Vec3 mean = scale(sum, 1.0f / static_cast<float>(kCaptureSamples));

    if (!validGravity(mean, 0.9f, 1.1f)) {
        result_ = CalResult::BadGravity;
        return;
    }
    for (size_t i = 0; i < kCaptureSamples; ++i) {
        if (angleBetweenDeg(samples_[i], mean) > kMovingLimitDeg) {
            result_ = CalResult::Moving;
            return;
        }
    }

    if (step_ == CalStep::Horizontal) {
        pendingG0_ = mean;
        horizontalDone_ = true;
        result_ = CalResult::Ok;
        return;
    }

    const float stepDeg = angleBetweenDeg(pendingG0_, mean);
    if (stepDeg < kMinStepDeg) {
        result_ = CalResult::TooSmallAngle;
        return;
    }
    if (stepDeg > kMaxStepDeg) {
        result_ = CalResult::TooLargeAngle;
        return;
    }
    cal_.g0 = pendingG0_;
    cal_.g1 = mean;
    applyCalibration();
    result_ = CalResult::Ok;
}

void AngleProvider::applyCalibration() {
    ref_ = unit(cal_.g0);
    axis_ = unit(cross(cal_.g0, cal_.g1));
    calibrated_ = true;
    windowCount_ = 0;
    windowPos_ = 0;
}

bool AngleProvider::saveTo(hal::ISettingsStore& store) const {
    return calibrated_ && store.save(kStoreKey, &cal_, sizeof(cal_));
}

bool AngleProvider::loadFrom(hal::ISettingsStore& store) {
    AngleCalibration loaded{};
    if (!store.load(kStoreKey, &loaded, sizeof(loaded))) return false;
    if (!validGravity(loaded.g0, 0.9f, 1.1f) || !validGravity(loaded.g1, 0.9f, 1.1f)) return false;
    const float stepDeg = angleBetweenDeg(loaded.g0, loaded.g1);
    if (stepDeg < kMinStepDeg || stepDeg > kMaxStepDeg) return false;
    cal_ = loaded;
    applyCalibration();
    return true;
}

}  // namespace angle
