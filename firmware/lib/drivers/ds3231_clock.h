#pragma once
#include <hal.h>

namespace drivers {

// DS3231 at 0x68 on the shared I2C bus; always holds UTC (cycle 007, R3/R4).
// lostPower() is true when the oscillator-stop flag is set (dead battery, first start)
// or when the chip does not answer or holds an impossible date.
class Ds3231Clock : public hal::IClock {
public:
    static constexpr uint8_t kAddress = 0x68;

    int64_t utc() const override;
    bool setUtc(int64_t epochSec) override;
    bool lostPower() const override;
    bool present() const;

private:
    bool readRegs(uint8_t reg, uint8_t* buf, size_t len) const;
    mutable uint32_t failedAtMs_ = 0;
};

}  // namespace drivers
