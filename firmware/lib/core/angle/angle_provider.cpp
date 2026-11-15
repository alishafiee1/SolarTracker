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
constexpr const char* kOffsetKey = "angle_off";
constexpr float kFaceMinG = 0.7f;     // cycle 018 R1: the dominant axis must carry at least this much gravity
constexpr float kFaceOtherG = 0.35f;  // ... and each of the other two at most this much (within ~20 degrees of a face)

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

Vec3 AngleProvider::correct(const Vec3& raw) const {
    return {(raw.x - offsets_.offset[0]) / offsets_.scale[0], (raw.y - offsets_.offset[1]) / offsets_.scale[1],
            (raw.z - offsets_.offset[2]) / offsets_.scale[2]};
}

void AngleProvider::sample() {
    Vec3 raw{0, 0, 0};
    const bool readOk = accel_.read(raw);
    if (readOk) lastRaw_ = raw;

    if (capturing_ && faceCapture_) {  // the six-face measurement works on the raw reading (cycle 018 R1)
        if (!readOk || !validGravity(raw, 0.7f, 1.3f)) {
            capturing_ = false;
            result_ = CalResult::SensorFault;
            return;
        }
        samples_[sampleCount_++] = raw;
        if (sampleCount_ == kCaptureSamples) finishFaceCapture();
        return;
    }

    const Vec3 g = readOk ? correct(raw) : Vec3{0, 0, 0};
    bool valid = readOk && validGravity(g, 0.8f, 1.2f);
    if (valid) lastG_ = g;

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

    // A sensor that gives no data or impossible data is a fault. A good reading whose angle is beyond +-60 degrees is only
    // a warning (cycle 019): the installer may be turning the box in his hands.
    if (!valid) {
        hasLive_ = false;
        if (invalidRun_ < 255) ++invalidRun_;
        if (invalidRun_ >= kFaultAfterInvalid) {
            fault_ = true;
            windowCount_ = 0;
            windowPos_ = 0;
        }
        return;
    }
    invalidRun_ = 0;
    if (!calibrated_) {
        hasLive_ = false;
        outOfRange_ = false;
        outRun_ = inRun_ = 0;
        return;
    }

    float deg = 0.0f;
    const bool defined = angleOf(g, deg);
    hasLive_ = defined;
    liveAngle_ = deg;
    const bool out = !defined || fabsf(deg) > kImplausibleDeg;
    if (out) {
        inRun_ = 0;
        if (outRun_ < 255) ++outRun_;
        if (outRun_ >= kWarnSamples && !outOfRange_) {
            outOfRange_ = true;
            windowCount_ = 0;  // the average restarts when the angle is normal again
            windowPos_ = 0;
        }
        return;
    }
    outRun_ = 0;
    if (inRun_ < 255) ++inRun_;
    if (inRun_ >= kWarnSamples) outOfRange_ = false;
    if (fault_) return;
    pushAngle(deg);
}

float AngleProvider::angleDeg() const {
    if (outOfRange_ && hasLive_) return liveAngle_;
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
    faceCapture_ = false;
    step_ = step;
    result_ = CalResult::Pending;
    sampleCount_ = 0;
    if (step == CalStep::Horizontal) {
        // A new horizontal capture starts a new calibration.
        horizontalDone_ = false;
        calibrated_ = false;
        windowCount_ = 0;
        windowPos_ = 0;
        outOfRange_ = false;
        hasLive_ = false;
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

bool AngleProvider::beginFaceCapture() {
    capturing_ = true;
    faceCapture_ = true;
    result_ = CalResult::Pending;
    sampleCount_ = 0;
    return true;
}

void AngleProvider::finishFaceCapture() {
    capturing_ = false;
    faceCapture_ = false;

    Vec3 sum{0, 0, 0};
    for (size_t i = 0; i < kCaptureSamples; ++i) sum = {sum.x + samples_[i].x, sum.y + samples_[i].y, sum.z + samples_[i].z};
    const Vec3 mean = scale(sum, 1.0f / static_cast<float>(kCaptureSamples));

    if (!validGravity(mean, 0.7f, 1.3f)) {
        result_ = CalResult::BadGravity;
        return;
    }
    for (size_t i = 0; i < kCaptureSamples; ++i) {
        if (angleBetweenDeg(samples_[i], mean) > kMovingLimitDeg) {
            result_ = CalResult::Moving;
            return;
        }
    }

    const float v[3] = {mean.x, mean.y, mean.z};
    int a = 0;
    for (int k = 1; k < 3; ++k)
        if (fabsf(v[k]) > fabsf(v[a])) a = k;
    bool onFace = fabsf(v[a]) >= kFaceMinG;
    for (int k = 0; k < 3; ++k)
        if (k != a && fabsf(v[k]) > kFaceOtherG) onFace = false;
    if (!onFace) {
        result_ = CalResult::BadFace;
        return;
    }
    const int face = 2 * a + (v[a] < 0.0f ? 1 : 0);
    faces_[face] = mean;
    faceMask_ = static_cast<uint8_t>(faceMask_ | (1u << face));
    result_ = CalResult::Ok;
}

void AngleProvider::invalidateAngleCalibration(hal::ISettingsStore& store) {
    const AngleCalibration none{};  // zeros fail the |g| check in loadFrom, so the old calibration is gone after a reboot too
    store.save(kStoreKey, &none, sizeof(none));
    calibrated_ = false;
    horizontalDone_ = false;
    windowCount_ = 0;
    windowPos_ = 0;
    outOfRange_ = false;
    hasLive_ = false;
}

bool AngleProvider::saveOffsets(hal::ISettingsStore& store) {
    if (faceMask_ != 0x3F) return false;
    AngleOffsets o{};
    for (int k = 0; k < 3; ++k) {
        const float* p = &faces_[2 * k].x;      // reading with +axis up
        const float* n = &faces_[2 * k + 1].x;  // reading with -axis up
        o.offset[k] = (p[k] + n[k]) / 2.0f;
        o.scale[k] = (p[k] - n[k]) / 2.0f;
        if (!(o.scale[k] >= 0.8f && o.scale[k] <= 1.2f)) {
            result_ = CalResult::BadGravity;
            return false;
        }
    }
    if (!store.save(kOffsetKey, &o, sizeof(o))) return false;
    offsets_ = o;
    offsetsSaved_ = true;
    invalidateAngleCalibration(store);
    return true;
}

bool AngleProvider::resetOffsets(hal::ISettingsStore& store) {
    const AngleOffsets none{};  // zero scale is invalid, so loadFrom ignores it
    const bool saved = store.save(kOffsetKey, &none, sizeof(none));
    offsets_ = AngleOffsets{{0, 0, 0}, {1, 1, 1}};
    offsetsSaved_ = false;
    faceMask_ = 0;
    invalidateAngleCalibration(store);
    return saved;
}

void AngleProvider::applyCalibration() {
    ref_ = unit(cal_.g0);
    axis_ = unit(cross(cal_.g0, cal_.g1));
    calibrated_ = true;
    windowCount_ = 0;
    windowPos_ = 0;
    outOfRange_ = false;
    outRun_ = inRun_ = 0;
}

bool AngleProvider::saveTo(hal::ISettingsStore& store) const {
    return calibrated_ && store.save(kStoreKey, &cal_, sizeof(cal_));
}

bool AngleProvider::loadFrom(hal::ISettingsStore& store) {
    AngleOffsets off{};  // the offsets come first: the angle calibration was taken in the corrected frame
    if (store.load(kOffsetKey, &off, sizeof(off))) {
        bool good = true;
        for (int k = 0; k < 3; ++k) good = good && isfinite(off.offset[k]) && off.scale[k] >= 0.8f && off.scale[k] <= 1.2f;
        if (good) {
            offsets_ = off;
            offsetsSaved_ = true;
        }
    }
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
