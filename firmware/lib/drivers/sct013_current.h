#pragma once
#include <hal.h>

namespace drivers {

// SCT-013 current transformer on GPIO 32 (ADC1_CH4, 1.65 V bias), module 009 R1. A low-priority task
// samples the input for 200 ms every 500 ms (about 10 mains cycles), removes the measured bias,
// computes the RMS and multiplies it by the calibration factor (amps per volt). The control tick
// only reads the last result, so it never waits for the sampling.
class Sct013Current : public hal::ICurrentSensor {
public:
    static constexpr int kPin = 32;

    // Starts the task once; later calls only update the factor.
    void begin(float ampsPerVolt);
    bool present() const override { return started_; }
    float rmsAmps() const override { return rms_; }
    float rmsVolts() const { return volts_; }

private:
    static void task(void* self);
    bool started_ = false;
    volatile float rms_ = 0.0f;
    volatile float volts_ = 0.0f;
    volatile float factor_ = 5.0f;
};

}  // namespace drivers
