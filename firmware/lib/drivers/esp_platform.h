#pragma once
#include <hal.h>

#include "boot_info.h"

namespace drivers {

class EspTime : public hal::ITime {
public:
    uint32_t nowMs() const override;
};

// Hardware random bytes (session tokens, password salts).
class EspRandom : public hal::IRandom {
public:
    void bytes(uint8_t* out, size_t len) override;
};

core::ResetReason readResetReason();

// R5: task watchdog on the loop task. The loop must call feedWatchdog() at
// least once per timeout; a hang resets the chip with relays off (R4).
void startWatchdog(uint32_t timeoutSec);
void feedWatchdog();

}  // namespace drivers
