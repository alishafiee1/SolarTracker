// SunTracker firmware entry point: wiring only, logic lives in lib/core.
#include <Arduino.h>
#include <Wire.h>

#include "angle_provider.h"
#include "boot_info.h"
#include "control_loop.h"
#include "ds3231_clock.h"
#include "esp_platform.h"
#include "littlefs_event_log.h"
#include "motion_controller.h"
#include "mpu6050_accel.h"
#include "nvs_settings_store.h"
#include "relays_gpio.h"
#include "settings.h"
#include "supply_watch.h"
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
constexpr int kPinSupply = 34;
// Supply divider on the board (5 V rail to the ADC). The real ratio is fixed by the PCB (hardware
// phase); below 1 V at the pin the divider is treated as not fitted and the watch stays off.
constexpr float kSupplyDividerRatio = 2.0f;
constexpr float kSupplyPinPresentV = 1.0f;

drivers::EspTime espTime;
drivers::RelaysGpio relays;
drivers::Mpu6050Accel accel;
drivers::Ds3231Clock rtcClock;
drivers::NvsSettingsStore nvs;
drivers::LittleFsEventLog eventLog(rtcClock, espTime);
settings::SettingsStore settingsStore(nvs);
settings::Settings cfg;
settings::SupplyWatch supplyWatch;
angle::AngleProvider angleProvider(accel);
motion::MotionController* motionCtl = nullptr;  // created in setup(), after the relays are forced off
core::ControlLoop controlLoop;
core::TickScheduler controlTick(0, core::kControlPeriodMs);
core::ResetReason resetReason = core::ResetReason::Unknown;
uint32_t lastSupplyMs = 0;

motion::Timing timingFrom(const settings::Settings& s) {
    motion::Timing t;
    t.stopSettleMs = s.stopSettleMs;
    t.minMoveGapMs = s.minMoveGapMs;
    t.maxMoveMs = s.maxMoveMs;
    return t;
}

float supplyVolts() { return analogReadMilliVolts(kPinSupply) / 1000.0f * kSupplyDividerRatio; }

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

    if (!eventLog.begin()) Serial.println("E event log: flash not usable, events are not stored");
    settingsStore.load(cfg, &eventLog);  // defaults + an event if nothing valid is stored
    eventLog.append(hal::EventType::PowerUp, core::resetReasonName(resetReason));  // R7

    motionCtl = new motion::MotionController(relays, timingFrom(cfg), espTime.nowMs());  // R9: starts settling
    motionCtl->setEventLog(&eventLog);

    controlTick = core::TickScheduler(espTime.nowMs(), core::kControlPeriodMs);
    lastSupplyMs = espTime.nowMs();
}

void loop() {
    drivers::feedWatchdog();
    const uint32_t now = espTime.nowMs();
    if (controlTick.due(now)) {
        angleProvider.sample();
        motionCtl->tick(now, angleProvider.angleDeg(), 0.0f);
        controlLoop.tick(now);
    }
    if (now - lastSupplyMs >= 1000) {  // R8
        lastSupplyMs = now;
        const float volts = supplyVolts();
        if (volts / kSupplyDividerRatio >= kSupplyPinPresentV && supplyWatch.onSecond(volts, cfg.supplyLowV)) {
            char detail[40];
            snprintf(detail, sizeof(detail), "supply %.2f V below %.2f V", static_cast<double>(volts), static_cast<double>(cfg.supplyLowV));
            eventLog.append(hal::EventType::SupplyLow, detail);
        }
    }
    eventLog.service();
#ifdef BENCH_CONSOLE
    drivers::benchConsolePoll({FW_VERSION, core::resetReasonName(resetReason), now, controlLoop.ticks(),
                               controlLoop.lastTickMs(), &accel, &angleProvider, motionCtl, &cfg, &settingsStore, &rtcClock,
                               &eventLog});
#endif
    delay(1);
}
