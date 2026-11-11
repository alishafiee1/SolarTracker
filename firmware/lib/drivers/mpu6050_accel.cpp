#include "mpu6050_accel.h"

#include <Arduino.h>
#include <Wire.h>

namespace drivers {

namespace {
constexpr uint8_t kRegConfig = 0x1A;
constexpr uint8_t kRegAccelConfig = 0x1C;
constexpr uint8_t kRegAccelConfig2 = 0x1D;  // MPU6500/9250 only: accelerometer low-pass filter
constexpr uint8_t kRegAccelXout = 0x3B;
constexpr uint8_t kRegPwrMgmt1 = 0x6B;
constexpr uint8_t kRegWhoAmI = 0x75;
constexpr float kLsbPerG = 16384.0f;          // +-2 g range
constexpr uint32_t kIdentityCheckMs = 10000;  // R1: WHO_AM_I every 10 s
constexpr uint32_t kRetryMs = 1000;

bool writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(Mpu6050Accel::kAddress);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(Mpu6050Accel::kAddress);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<int>(Mpu6050Accel::kAddress), static_cast<int>(len)) != static_cast<int>(len)) return false;
    for (size_t i = 0; i < len; ++i) buf[i] = static_cast<uint8_t>(Wire.read());
    return true;
}
}  // namespace

bool Mpu6050Accel::checkIdentity() {
    uint8_t id = 0;
    whoAmI_ = readRegs(kRegWhoAmI, &id, 1) ? id : 0;
    lastCheckMs_ = millis();
    // Many cheap GY-521 boards carry an MPU6500/9250 clone: same accelerometer registers, other ID.
    return whoAmI_ == kWhoAmIExpected || whoAmI_ == 0x70 || whoAmI_ == 0x71 || whoAmI_ == 0x73;
}

bool Mpu6050Accel::begin() {
    lastBeginMs_ = millis();
    configured_ = false;
    ok_ = false;
    if (!checkIdentity()) return false;
    // Wake up, accelerometer +-2 g, digital low-pass filter about 21 Hz.
    // The MPU6500 filters the accelerometer in ACCEL_CONFIG2 (0x04 = 21 Hz); without it the noise is about 1 degree.
    // On a real MPU6050 that register does not exist and the write is ignored.
    configured_ = writeReg(kRegPwrMgmt1, 0x00) && writeReg(kRegAccelConfig, 0x00) && writeReg(kRegConfig, 0x04) &&
                  writeReg(kRegAccelConfig2, 0x04);
    ok_ = configured_;
    return ok_;
}

bool Mpu6050Accel::read(hal::Vec3& g) {
    if (!configured_) {
        // A missing sensor is retried once a second, not on every tick (each try logs an I2C error).
        if (millis() - lastBeginMs_ < kRetryMs) return false;
        lastBeginMs_ = millis();
        if (!begin()) return false;
    }
    if (millis() - lastCheckMs_ >= kIdentityCheckMs && !checkIdentity()) {
        ok_ = false;
        configured_ = false;
        return false;
    }
    uint8_t raw[6];
    if (!readRegs(kRegAccelXout, raw, sizeof(raw))) {
        ok_ = false;
        configured_ = false;  // re-initialise when the sensor comes back
        return false;
    }
    auto toInt = [&](int i) { return static_cast<int16_t>((raw[i] << 8) | raw[i + 1]); };
    g = {toInt(0) / kLsbPerG, toInt(2) / kLsbPerG, toInt(4) / kLsbPerG};
    ok_ = true;
    return true;
}

}  // namespace drivers
