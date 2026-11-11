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

struct AngleCalibration {
    hal::Vec3 g0;  // gravity with the panel horizontal
    hal::Vec3 g1;  // gravity with the panel turned east
    uint32_t createdUtc;
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
};

class AngleProvider {
public:
    explicit AngleProvider(hal::IAccel& accel) : accel_(accel) {}

    // Call once per control tick (10 Hz).
    void sample();

    // Moving average of the last 5 valid samples, degrees. 0 when not calibrated.
    float angleDeg() const;
    bool calibrated() const { return calibrated_; }
    // Last valid raw gravity reading (g), so the panel can show the sensor is alive before calibration.
    const hal::Vec3& lastGravity() const { return lastG_; }
    // False after a latched sensor fault (R5) until clearFault().
    bool ok() const { return !fault_; }
    void clearFault();

    // Non-blocking calibration capture: begin, keep calling sample(), poll captureResult().
    // East needs a finished Horizontal capture first (returns false otherwise).
    bool beginCapture(CalStep step);
    void cancelCapture();
    bool capturing() const { return capturing_; }
    CalResult captureResult() const { return result_; }

    const AngleCalibration& calibration() const { return cal_; }
    void setCreatedUtc(uint32_t utc) { cal_.createdUtc = utc; }

    // R7: persistence through the settings store.
    bool saveTo(hal::ISettingsStore& store) const;
    bool loadFrom(hal::ISettingsStore& store);

private:
    static bool validGravity(const hal::Vec3& g, float lo, float hi);
    void finishCapture();
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
    uint8_t invalidRun_ = 0;
    bool fault_ = false;

    bool capturing_ = false;
    CalStep step_ = CalStep::Horizontal;
    CalResult result_ = CalResult::NotCaptured;
    bool horizontalDone_ = false;
    hal::Vec3 pendingG0_{0, 0, 0};
    hal::Vec3 samples_[kCaptureSamples];
    size_t sampleCount_ = 0;
};

}  // namespace angle
