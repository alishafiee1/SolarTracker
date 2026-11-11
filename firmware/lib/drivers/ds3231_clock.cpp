#include "ds3231_clock.h"

#include <Arduino.h>
#include <Wire.h>

#include "rtc_time.h"

namespace drivers {

namespace {
constexpr uint8_t kRegSeconds = 0x00;
// The fitted chip is a DS1307 (HW-111 module). Its register 0x0F is plain RAM, so the DS3231 "oscillator
// stopped" flag cannot be used; the DS1307 equivalent is the CH (clock halt) bit, bit 7 of the seconds register.
constexpr uint8_t kClockHalt = 0x80;
}  // namespace

bool Ds3231Clock::readRegs(uint8_t reg, uint8_t* buf, size_t len) const {
    // A missing chip is retried once a second: every failed try logs an I2C error and costs time.
    if (failedAtMs_ != 0 && millis() - failedAtMs_ < 1000) return false;
    Wire.beginTransmission(kAddress);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(static_cast<int>(kAddress), static_cast<int>(len)) != static_cast<int>(len)) {
        failedAtMs_ = millis() | 1;  // never 0, which means "no failure yet"
        return false;
    }
    failedAtMs_ = 0;
    for (size_t i = 0; i < len; ++i) buf[i] = static_cast<uint8_t>(Wire.read());
    return true;
}

bool Ds3231Clock::present() const {
    uint8_t seconds;
    return readRegs(kRegSeconds, &seconds, 1);
}

bool Ds3231Clock::lostPower() const {
    uint8_t seconds;
    if (!readRegs(kRegSeconds, &seconds, 1)) return true;
    if (seconds & kClockHalt) return true;  // stopped: battery was empty or the time was never set
    return utc() == 0;  // unreadable or implausible date
}

int64_t Ds3231Clock::utc() const {
    uint8_t r[7];
    if (!readRegs(kRegSeconds, r, sizeof(r))) return 0;
    rtc::DateTime dt;
    dt.second = rtc::fromBcd(r[0] & 0x7F);
    dt.minute = rtc::fromBcd(r[1] & 0x7F);
    dt.hour = rtc::fromBcd(r[2] & 0x3F);  // 24 h mode
    dt.day = rtc::fromBcd(r[4] & 0x3F);
    dt.month = rtc::fromBcd(r[5] & 0x1F);
    dt.year = 2000 + rtc::fromBcd(r[6]);
    return rtc::isPlausible(dt) ? rtc::toEpoch(dt) : 0;
}

bool Ds3231Clock::setUtc(int64_t epochSec) {
    const rtc::DateTime dt = rtc::fromEpoch(epochSec);
    if (!rtc::isPlausible(dt)) return false;
    Wire.beginTransmission(kAddress);
    Wire.write(kRegSeconds);
    Wire.write(rtc::toBcd(dt.second));
    Wire.write(rtc::toBcd(dt.minute));
    Wire.write(rtc::toBcd(dt.hour));
    Wire.write(rtc::toBcd(1));  // weekday is not used
    Wire.write(rtc::toBcd(dt.day));
    Wire.write(rtc::toBcd(dt.month));
    Wire.write(rtc::toBcd(dt.year - 2000));
    // Seconds are written with bit 7 clear, which also starts a halted DS1307.
    return Wire.endTransmission() == 0;
}

}  // namespace drivers
