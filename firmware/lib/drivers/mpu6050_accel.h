#pragma once
#include <hal.h>

namespace drivers {

// MPU6050 on the shared I2C bus, address 0x69 (AD0 tied to 3.3 V; the DS3231 uses 0x68).
class Mpu6050Accel : public hal::IAccel {
public:
    static constexpr uint8_t kAddress = 0x69;
    static constexpr uint8_t kWhoAmIExpected = 0x68;  // MPU6050; 0x70/0x71/0x73 (MPU6500/9250) are accepted too

    // Wire must be started by the caller (SDA 21, SCL 22).
    bool begin();
    bool read(hal::Vec3& g) override;
    bool ok() const override { return ok_; }

    // Last raw WHO_AM_I value seen (0 = no answer), for the bench console.
    uint8_t lastWhoAmI() const { return whoAmI_; }

private:
    bool checkIdentity();

    bool ok_ = false;
    bool configured_ = false;
    uint8_t whoAmI_ = 0;
    uint32_t lastCheckMs_ = 0;
    uint32_t lastBeginMs_ = 0;
};

}  // namespace drivers
