// SunTracker firmware entry point: wiring only, logic lives in lib/core.
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>

#include "angle_provider.h"
#include "anemometer.h"
#include "boot_info.h"
#include "control_loop.h"
#include "control_state_machine.h"
#include "ds3231_clock.h"
#include "esp_platform.h"
#include "littlefs_event_log.h"
#include "motion_controller.h"
#include "mpu6050_accel.h"
#include "nvs_settings_store.h"
#include "ota_update.h"
#include "panel_server.h"
#include "relays_gpio.h"
#include "site_net.h"
#include "sct013_current.h"
#include "settings.h"
#include "supply_watch.h"
#include "tick_scheduler.h"
#include "web_api.h"
#include "wind_monitor.h"
#ifdef BENCH_CONSOLE
#include "bench_console.h"
#endif

// Default passwords come from an untracked secrets.h (see secrets.example.h); the placeholders are public.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef DEFAULT_AP_PASSWORD
#define DEFAULT_AP_PASSWORD "suntracker-ap-dev"
#endif
#ifndef DEFAULT_PANEL_PASSWORD
#define DEFAULT_PANEL_PASSWORD "suntracker-dev"
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
drivers::EspRandom espRandom;
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
control::ControlStateMachine* controller = nullptr;
drivers::Anemometer anemometer;
drivers::OtaUpdate otaUpdate;
drivers::SiteNet siteNet;
net::NetConfig netCfg;
control::FaultCode lastNotifiedFault = control::FaultCode::None;
bool panelStarted = false;
drivers::Sct013Current currentSensor;
wind::WindMonitor windMonitor;
webapi::Auth auth(espRandom);
webapi::WebApi* webApi = nullptr;
web::PanelServer* panel = nullptr;
int64_t cachedUtc = 0;       // the DS3231 is read once a second, in between the time is extrapolated
uint32_t cachedUtcAtMs = 0;
bool cachedClockValid = false;
core::ControlLoop controlLoop;
core::TickScheduler controlTick(0, core::kControlPeriodMs);
core::ResetReason resetReason = core::ResetReason::Unknown;
uint32_t lastSupplyMs = 0;

motion::Timing timingFrom(const settings::Settings& s) {
    motion::Timing t;
    t.stopSettleMs = s.stopSettleMs;
    t.minMoveGapMs = s.minMoveGapMs;
    t.maxMoveMs = s.maxMoveMs;
    t.currentPresent = s.currentPresent;
    t.zeroCurrentA = s.zeroCurrentA;
    return t;
}

float supplyVolts() { return analogReadMilliVolts(kPinSupply) / 1000.0f * kSupplyDividerRatio; }

int64_t utcNow() { return cachedClockValid ? cachedUtc + (espTime.nowMs() - cachedUtcAtMs) / 1000 : 0; }

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
    angleProvider.loadFrom(nvs);         // the calibration, if one was saved
    otaUpdate.bootCheck(&eventLog);      // probation after an update; rolls back and restarts if the new image keeps failing
    eventLog.append(hal::EventType::PowerUp, core::resetReasonName(resetReason));  // R7

    motionCtl = new motion::MotionController(relays, timingFrom(cfg), espTime.nowMs());  // R9: starts settling
    motionCtl->setEventLog(&eventLog);
    controller = new control::ControlStateMachine(*motionCtl, cfg, &eventLog);
    anemometer.begin();
    if (cfg.currentPresent) currentSensor.begin(cfg.currentFactor);  // module 009

    net::load(nvs, netCfg);  // site Wi-Fi and Bale settings (module 011), empty until configured
    siteNet.apply(netCfg);

    webapi::Deps deps(*controller, cfg, settingsStore, nvs, angleProvider, rtcClock, eventLog, windMonitor, auth);
    deps.fwVersion = FW_VERSION;
    deps.defaultPassword = DEFAULT_PANEL_PASSWORD;
    deps.clockValid = [] { return cachedClockValid; };
    deps.utcNow = utcNow;
    deps.readLog = [](char* buf, size_t len, size_t n) { return eventLog.readLatest(buf, len, n); };
    deps.onNetworkChanged = [] {
        net::load(nvs, netCfg);
        siteNet.apply(netCfg);
    };
    deps.netInfo = [] {
        webapi::NetInfo n;
        n.connected = siteNet.connected();
        snprintf(n.ip, sizeof(n.ip), "%s", siteNet.ip());
        n.lastNotify = siteNet.lastNotify();
        return n;
    };
    deps.sendTestMessage = [] {  // queued, not sent here: a request must never wait for the internet
        return siteNet.notify("test:" + std::to_string(millis()), "SunTracker: پیام آزمایشی", millis());  // a unique key: tests are never merged
    };
    deps.startWifiScan = [] {
        if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
            WiFi.scanDelete();
            WiFi.scanNetworks(true);  // asynchronous: returns at once
        }
    };
    deps.wifiScan = [] {
        webapi::WifiScan s;
        const int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) {
            s.state = 1;
        } else if (n >= 0) {
            s.state = 2;
            for (int i = 0; i < n; ++i) s.networks.push_back({WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
            net::tidyScan(s.networks);
        }
        return s;
    };
    deps.logBytes = [] { return eventLog.storedBytes(); };
    deps.onSettingsChanged = [] {
        motionCtl->setTiming(timingFrom(cfg));
        if (cfg.currentPresent) currentSensor.begin(cfg.currentFactor);
    };
    webApi = new webapi::WebApi(deps);
    panel = new web::PanelServer(*webApi);
    panel->setLogCsv(&eventLog, [] { return static_cast<int>(cfg.tzOffsetMin); }, utcNow);
    const char* apPassword = strlen(cfg.apPassword) >= 8 ? cfg.apPassword : DEFAULT_AP_PASSWORD;
    panel->setOta(&otaUpdate, &eventLog);
    if (panel->begin(apPassword)) {
        panelStarted = true;
        Serial.printf("I hotspot %s up, panel at http://192.168.4.1/\n", panel->ssid());
    } else {
        Serial.println("E hotspot failed to start; the controller keeps working without the panel");  // R8
    }

    controlTick = core::TickScheduler(espTime.nowMs(), core::kControlPeriodMs);
    lastSupplyMs = espTime.nowMs();
}

void loop() {
    drivers::feedWatchdog();
    const uint32_t now = espTime.nowMs();
    panel->lock();  // the web handlers run in another task; they see the control state only between ticks
    const bool tickDue = controlTick.due(now);
    if (tickDue && otaUpdate.updating()) {
        // While a firmware image is written the actuator stays off and nothing new is started (module 010 R2).
        if (motionCtl->running()) motionCtl->emergencyStop(now, "firmware update");
    } else if (tickDue) {
        angleProvider.sample();
        if (now - cachedUtcAtMs >= 1000 || cachedUtcAtMs == 0) {
            cachedClockValid = !rtcClock.lostPower();
            cachedUtc = cachedClockValid ? rtcClock.utc() : 0;
            cachedUtcAtMs = now;
        }
        control::Inputs in;
        in.angleDeg = angleProvider.angleDeg();
        in.angleOk = angleProvider.ok();
        in.calibrated = angleProvider.calibrated();
        in.clockValid = cachedClockValid;
        in.utcEpoch = utcNow();
        uint32_t pulses[16];
        for (size_t n; (n = anemometer.drain(pulses, 16)) > 0;)
            for (size_t i = 0; i < n; ++i) windMonitor.feed(pulses[i]);
        const wind::Config wc{cfg.anemometerFactor, cfg.windStowKmh, cfg.windStowDelayS, cfg.windReturnKmh, cfg.windReturnMin};
        const wind::Result wr = windMonitor.update(now, micros(), wc);
        if (wr.stowStarted || wr.stowEnded) {
            char detail[48];
            snprintf(detail, sizeof(detail), "wind stow %s, peak %.0f km/h", wr.stowStarted ? "start" : "end", static_cast<double>(wr.peakKmh));
            eventLog.append(hal::EventType::StateChange, detail);
            if (wr.stowStarted) siteNet.notify("wind", net::formatNotification(panel->ssid(), "باد شدید — حالت ایمن", utcNow(), cfg.tzOffsetMin), now);
        }
        in.windStow = windMonitor.stow();
        in.currentA = cfg.currentPresent ? currentSensor.rmsAmps() : 0.0f;  // optional module 009
#ifdef BENCH_CONSOLE
        if (drivers::benchDirectMotion()) {
            motionCtl->tick(now, in.angleDeg, in.currentA);  // relay bench tests drive the motion layer directly
        } else
#endif
        controller->tick(now, in);  // also ticks the MotionController
        controlLoop.tick(now);
        const control::Status st = controller->status(now);  // a new fault goes to the phone (module 011)
        if (st.fault != lastNotifiedFault) {
            lastNotifiedFault = st.fault;
            if (st.fault != control::FaultCode::None) {
                siteNet.notify(std::string("fault:") + control::faultName(st.fault), net::formatNotification(panel->ssid(), webapi::statusTextFa(st.text, 0).c_str(), utcNow(), cfg.tzOffsetMin), now);
            }
        }
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
    siteNet.service(now, rtcClock, cachedClockValid, &eventLog);
    otaUpdate.confirmIfHealthy(now, panelStarted && controlLoop.ticks() > 100, &eventLog);
    if (otaUpdate.restartDue(now)) {
        eventLog.flush();
        ESP.restart();
    }
    panel->unlock();
    panel->service();  // DNS answers for the captive portal (outside the lock)
#ifdef BENCH_CONSOLE
    drivers::benchConsolePoll({FW_VERSION, core::resetReasonName(resetReason), now, controlLoop.ticks(),
                               controlLoop.lastTickMs(), &accel, &angleProvider, motionCtl, &cfg, &settingsStore, &rtcClock, controller,
                               &eventLog, &windMonitor, &anemometer, &currentSensor});
#endif
    delay(1);
}
