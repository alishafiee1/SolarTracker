// SunTracker firmware entry point: wiring only, logic lives in lib/core.
#include <Arduino.h>
#include <Wire.h>

#include "angle_provider.h"
#include "boot_info.h"
#include "control_loop.h"
#include "esp_platform.h"
#include "motion_controller.h"
#include "mpu6050_accel.h"
#include "relays_gpio.h"
#include "tick_scheduler.h"
#ifdef BENCH_CONSOLE
#include "bench_console.h"
#endif

#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif

namespace {

constexpr uint32_t kWatchdogTimeoutSec = 5;
constexpr int kPinI2cSda = 21;
constexpr int kPinI2cScl = 22;

drivers::EspTime espTime;
drivers::RelaysGpio relays;
drivers::Mpu6050Accel accel;
angle::AngleProvider angleProvider(accel);
motion::MotionController* motionCtl = nullptr;  // created in setup(), after the relays are forced off
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

    Wire.begin(kPinI2cSda, kPinI2cScl);
    Wire.setTimeOut(20);  // ms; a missing sensor must not stall the control tick
    accel.begin();

    motionCtl = new motion::MotionController(relays, motion::Timing(), espTime.nowMs());  // R9: starts settling
#ifdef BENCH_CONSOLE
    motionCtl->setEventLog(drivers::benchEventLog());
#endif

    controlTick = core::TickScheduler(espTime.nowMs(), core::kControlPeriodMs);
}

void loop() {
    drivers::feedWatchdog();
    const uint32_t now = espTime.nowMs();
    if (controlTick.due(now)) {
        angleProvider.sample();
        motionCtl->tick(now, angleProvider.angleDeg(), 0.0f);
        controlLoop.tick(now);
    }
#ifdef BENCH_CONSOLE
    drivers::benchConsolePoll({FW_VERSION, core::resetReasonName(resetReason), now, controlLoop.ticks(),
                               controlLoop.lastTickMs(), &accel, &angleProvider, motionCtl});
#endif
    delay(1);
}
