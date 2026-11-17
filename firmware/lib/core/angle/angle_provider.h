#pragma once
// Panel angle from the on-board accelerometer (cycle 003).
// Rotation R is positive towards west, 0 = horizontal. The two-step calibration
// (panel horizontal, then turned a few degrees EAST) finds the rotation plane and
// the sign, so the box can be mounted in any orientation.

#include <hal.h>
#include <stddef.h>
#include <stdint.h>

namespace angle {

constexpr size_t kAverageSamples = 5;     // R4: moving average length
constexpr size_t kCaptureSamples = 50;    // R3: 5 s at 10 Hz
constexpr uint8_t kFaultAfterInvalid = 5; // R5: consecutive bad samples (0.5 s at 10 Hz)
constexpr uint8_t kWarnSamples = 5;       // cycle 019 R2: consecutive samples to raise or clear the out-of-range warning

struct AngleCalibration {
    hal::Vec3 g0;  // gravity with the panel horizontal
    hal::Vec3 g1;  // gravity with the panel turned east
    uint32_t createdUtc;
};

// Per-axis correction of a cheap accelerometer, measured with the six-face procedure (cycle 018):
// corrected = (raw - offset) / scale. The identity (offset 0, scale 1) means "not measured".
struct AngleOffsets {
    float offset[3];
    float scale[3];
};

enum class CalStep : uint8_t { Horizontal, East };

enum class CalResult : uint8_t {
    Ok,
    Pending,        // capture still running
    Moving,         // panel moved during the capture
    TooSmallAngle,  // the two positions differ by less than 3 degrees
    TooLargeAngle,  // ... or by more than 30 degrees
    BadGravity,     // |g| outside 0.9..1.1
    SensorFault,
    NotCaptured,    // nothing was captured yet
    BadFace,        // six-face capture: the device does not stand on one of its six faces
};

class AngleProvider {
public:
    explicit AngleProvider(hal::IAccel& accel) : accel_(accel) {}

    // Call once per control tick (10 Hz).
    void sample();

    // Moving average of the last 5 valid samples, degrees. 0 when not calibrated. While outOfRange() it is the live angle.
    float angleDeg() const;
    // Cycle 019: a valid reading whose angle is beyond +-60 degrees (or undefined). Not a fault: it clears by itself.
    bool outOfRange() const { return outOfRange_; }
    // Live angle from the newest good reading, even out of range or while a fault is latched; false without a calibration.
    bool liveAngle(float& deg) const {
        deg = liveAngle_;
        return hasLive_;
    }
    bool calibrated() const { return calibrated_; }
    // Last valid corrected gravity reading (g).
    const hal::Vec3& lastGravity() const { return lastG_; }
    // Last reading straight from the sensor (g), so the panel can show the live face before any offset exists.
    const hal::Vec3& lastGravityRaw() const { return lastRaw_; }
    // False after a latched sensor fault (R5) until clearFault().
    bool ok() const { return !fault_; }
    // Cycle 033: why the data was bad the last time ("" = never). "i2c" = the sensor did not answer, "gravity" = an impossible |g|.
    const char* faultCause() const { return cause_ == 1 ? "i2c" : cause_ == 2 ? "gravity" : ""; }
    float lastBadMagnitude() const { return badMag_; }  // |g| of the last impossible sample
    void clearFault();

    // Non-blocking calibration capture: begin, keep calling sample(), poll captureResult().
    // East needs a finished Horizontal capture first (returns false otherwise).
    bool beginCapture(CalStep step);
    void cancelCapture();
    bool capturing() const { return capturing_; }
    CalResult captureResult() const { return result_; }

    // Six-face offset measurement (cycle 018). The face is detected from the reading itself.
    bool beginFaceCapture();
    uint8_t faceMask() const { return faceMask_; }  // bit f = 2*axis + (negative ? 1 : 0)
    // Needs all six faces. Stores the offsets and drops the angle calibration (its vectors belong to the old frame).
    bool saveOffsets(hal::ISettingsStore& store);
    bool resetOffsets(hal::ISettingsStore& store);
    bool offsetsSaved() const { return offsetsSaved_; }
    const AngleOffsets& offsets() const { return offsets_; }

    const AngleCalibration& calibration() const { return cal_; }
    void setCreatedUtc(uint32_t utc) { cal_.createdUtc = utc; }

    // R7: persistence through the settings store.
    bool saveTo(hal::ISettingsStore& store) const;
    bool loadFrom(hal::ISettingsStore& store);

private:
    static bool validGravity(const hal::Vec3& g, float lo, float hi);
    void finishCapture();
    void finishFaceCapture();
    void invalidateAngleCalibration(hal::ISettingsStore& store);
    hal::Vec3 correct(const hal::Vec3& raw) const;
    void applyCalibration();
    bool angleOf(const hal::Vec3& g, float& deg) const;
    void pushAngle(float deg);

    hal::IAccel& accel_;

    AngleCalibration cal_{};
    bool calibrated_ = false;
    hal::Vec3 axis_{0, 0, 0};  // unit rotation axis
    hal::Vec3 ref_{0, 0, 0};   // unit reference direction (horizontal gravity)

    float window_[kAverageSamples] = {};
    size_t windowCount_ = 0;
    size_t windowPos_ = 0;
    hal::Vec3 lastG_{0, 0, 0};
    hal::Vec3 lastRaw_{0, 0, 0};
    uint8_t invalidRun_ = 0;
    uint8_t cause_ = 0;
    float badMag_ = 0.0f;
    bool fault_ = false;
    uint8_t outRun_ = 0;
    uint8_t inRun_ = 0;
    bool outOfRange_ = false;
    bool hasLive_ = false;
    float liveAngle_ = 0.0f;

    AngleOffsets offsets_{{0, 0, 0}, {1, 1, 1}};
    bool offsetsSaved_ = false;
    hal::Vec3 faces_[6] = {};
    uint8_t faceMask_ = 0;
    bool faceCapture_ = false;

    bool capturing_ = false;
    CalStep step_ = CalStep::Horizontal;
    CalResult result_ = CalResult::NotCaptured;
    bool horizontalDone_ = false;
    hal::Vec3 pendingG0_{0, 0, 0};
    hal::Vec3 samples_[kCaptureSamples];
    size_t sampleCount_ = 0;
};

}  // namespace angle
