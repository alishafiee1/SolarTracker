#pragma once
#include <hal.h>

namespace drivers {

// Relay module inputs are active-low and pulled up on the board (proposal §04).
constexpr int kPinRelayPower = 25;      // CH1: actuator power ON/OFF
constexpr int kPinRelayDirection = 26;  // CH2: direction east/west

class RelaysGpio : public hal::IRelays {
public:
    // R4: call first thing in setup(). Drives both inputs HIGH (inactive)
    // before switching them to outputs, so they never pulse LOW during boot.
    static void forceOffAtBoot();

    void setPower(bool on) override;
    bool setDirection(hal::Dir dir) override;
    bool power() const override { return power_; }
    hal::Dir direction() const override { return dir_; }

private:
    bool power_ = false;
    hal::Dir dir_ = hal::Dir::East;
};

}  // namespace drivers
