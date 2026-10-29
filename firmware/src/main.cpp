// SunTracker firmware entry point: wiring only, logic lives in lib/core.
#include <Arduino.h>

#include "boot_info.h"
#include "control_loop.h"
#include "esp_platform.h"
#include "relays_gpio.h"
#include "tick_scheduler.h"

#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif

namespace {

constexpr uint32_t kWatchdogTimeoutSec = 5;

drivers::EspTime espTime;
drivers::RelaysGpio relays;
core::ControlLoop controlLoop;
core::TickScheduler controlTick(0, core::kControlPeriodMs);
core::ResetReason resetReason = core::ResetReason::Unknown;

}  // namespace

void setup() {
    // R4: relays off before anything else runs. Keep this the first line.
    drivers::RelaysGpio::forceOffAtBoot();

    drivers::startWatchdog(kWatchdogTimeoutSec);
    resetReason = drivers::readResetReason();

    Serial.begin(115200);
    char banner[96];
    core::formatBootBanner(banner, sizeof(banner), FW_VERSION, resetReason);
    Serial.printf("I %s\n", banner);

    controlTick = core::TickScheduler(espTime.nowMs(), core::kControlPeriodMs);
}

void loop() {
    drivers::feedWatchdog();
    const uint32_t now = espTime.nowMs();
    if (controlTick.due(now)) {
        controlLoop.tick(now);
    }
    delay(1);
}
