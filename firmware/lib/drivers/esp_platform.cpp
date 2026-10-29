#include "esp_platform.h"

#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

namespace drivers {

uint32_t EspTime::nowMs() const { return millis(); }

void EspRandom::bytes(uint8_t* out, size_t len) { esp_fill_random(out, len); }

core::ResetReason readResetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return core::ResetReason::PowerOn;
        case ESP_RST_EXT: return core::ResetReason::External;
        case ESP_RST_SW: return core::ResetReason::Software;
        case ESP_RST_PANIC: return core::ResetReason::Panic;
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT: return core::ResetReason::Watchdog;
        case ESP_RST_BROWNOUT: return core::ResetReason::Brownout;
        case ESP_RST_DEEPSLEEP: return core::ResetReason::DeepSleep;
        default: return core::ResetReason::Unknown;
    }
}

void startWatchdog(uint32_t timeoutSec) {
    esp_task_wdt_init(timeoutSec, true);  // panic â†’ reset
    esp_task_wdt_add(nullptr);            // current task (Arduino loop)
}

void feedWatchdog() { esp_task_wdt_reset(); }

}  // namespace drivers
