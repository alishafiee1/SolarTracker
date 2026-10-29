#include "relays_gpio.h"

#include <Arduino.h>

namespace drivers {

namespace {
constexpr int kActive = LOW;
constexpr int kInactive = HIGH;
// CH2 de-energised selects east, energised selects west.
}  // namespace

void RelaysGpio::forceOffAtBoot() {
    digitalWrite(kPinRelayPower, kInactive);
    digitalWrite(kPinRelayDirection, kInactive);
    pinMode(kPinRelayPower, OUTPUT);
    pinMode(kPinRelayDirection, OUTPUT);
}

void RelaysGpio::setPower(bool on) {
    digitalWrite(kPinRelayPower, on ? kActive : kInactive);
    power_ = on;
}

bool RelaysGpio::setDirection(hal::Dir dir) {
    if (power_) {
        return false;
    }
    digitalWrite(kPinRelayDirection, dir == hal::Dir::West ? kActive : kInactive);
    dir_ = dir;
    return true;
}

}  // namespace drivers
