#include "bench_console.h"

#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "csv_stream.h"
#include "esp_platform.h"
#include "relays_gpio.h"
#include "sun_position.h"
#include "tracking.h"

namespace drivers {

namespace {

class SerialEventLog : public hal::IEventLog {
public:
    void append(hal::EventType type, const char* detail) override {
        Serial.printf("I event: type=%d %s\n", static_cast<int>(type), detail);
    }
};
SerialEventLog serialEventLog;

bool directMotion = false;
uint32_t genHz = 0;       // pulse generator on the anemometer pin (bench only)
uint32_t genLastUs = 0;

char lineBuf[32];
size_t lineLen = 0;

void printPins() {
    Serial.printf("I pins: CH1(power)=%d CH2(direction)=%d  (1 = inactive)\n", digitalRead(kPinRelayPower),
                  digitalRead(kPinRelayDirection));
}

void handle(const char* cmd, const BenchStatus& s) {
    if (strcmp(cmd, "status") == 0) {
        Serial.printf("I status: version=%s reset=%s uptime_ms=%lu ticks=%lu last_tick_ms=%lu CH1=%d CH2=%d\n",
                      s.version, s.resetReason, static_cast<unsigned long>(s.uptimeMs),
                      static_cast<unsigned long>(s.ticks), static_cast<unsigned long>(s.lastTickMs),
                      digitalRead(kPinRelayPower), digitalRead(kPinRelayDirection));
    } else if (strncmp(cmd, "sun", 3) == 0) {
        // "sun" or "sun <utc epoch>": Kerman sun position, rotation and the time one call takes (cycle 002 R6).
        const int64_t epoch = cmd[3] == ' ' ? atoll(cmd + 4) : 1813561200LL;  // default: 2027-06-21 07:00 UTC
        const sun::SunPosition p = sun::position(epoch, 30.283, 57.083);
        volatile double sink = 0;
        const uint32_t t0 = micros();
        for (int i = 0; i < 100; ++i) {
            sink = sink + sun::position(epoch + i, 30.283, 57.083).zenithDeg;
        }
        const uint32_t usPerCall = (micros() - t0) / 100;
        Serial.printf("I sun: epoch=%lld azimuth=%.4f zenith=%.4f rotation=%.4f us_per_call=%lu\n",
                      static_cast<long long>(epoch), p.azimuthDeg, p.zenithDeg, tracking::rotation(p),
                      static_cast<unsigned long>(usPerCall));
    } else if (strcmp(cmd, "angle") == 0) {
        hal::Vec3 g{0, 0, 0};
        const bool read = s.accel->read(g);
        Serial.printf("I angle: whoami=0x%02X read=%d g=(%.3f,%.3f,%.3f) |g|=%.3f calibrated=%d angle_deg=%.2f ok=%d\n",
                      s.accel->lastWhoAmI(), read, g.x, g.y, g.z, sqrtf(g.x * g.x + g.y * g.y + g.z * g.z),
                      s.angle->calibrated(), s.angle->angleDeg(), s.angle->ok());
    } else if (strcmp(cmd, "cal h") == 0 || strcmp(cmd, "cal e") == 0) {
        const bool east = cmd[4] == 'e';
        const bool started = s.angle->beginCapture(east ? angle::CalStep::East : angle::CalStep::Horizontal);
        Serial.printf("%c cal: %s capture %s (5 s, keep the panel still)\n", started ? 'I' : 'E', east ? "east" : "horizontal",
                      started ? "started" : "refused: do the horizontal step first");
    } else if (strcmp(cmd, "cal") == 0) {
        static const char* const names[] = {"ok", "pending", "moving", "too-small-angle", "too-large-angle",
                                            "bad-gravity", "sensor-fault", "not-captured"};
        Serial.printf("I cal: capturing=%d result=%s calibrated=%d\n", s.angle->capturing(),
                      names[static_cast<int>(s.angle->captureResult())], s.angle->calibrated());
    } else if (strcmp(cmd, "move e") == 0 || strcmp(cmd, "move w") == 0) {
        directMotion = true;
        s.motion->request(motion::Cmd::move(cmd[5] == 'e' ? hal::Dir::East : hal::Dir::West));
        Serial.printf("I motion: requested move %s\n",cmd[5] == 'e' ? "east" : "west");
    } else if (strcmp(cmd, "stop") == 0) {
        directMotion = true;
        s.motion->request(motion::Cmd::stop());
        Serial.println("I motion: requested stop");
    } else if (strcmp(cmd, "motion") == 0) {
        static const char* const states[] = {"idle", "selecting", "running", "settling"};
        Serial.printf("I motion: state=%s dir=%s wait_ms=%lu fault=%d CH1=%d CH2=%d\n",states[static_cast<int>(s.motion->state())],
                      s.motion->direction() == hal::Dir::West ? "west" : "east",
                      static_cast<unsigned long>(s.motion->safetyWaitMs(s.uptimeMs)), static_cast<int>(s.motion->fault()),
                      digitalRead(kPinRelayPower), digitalRead(kPinRelayDirection));
    } else if (strcmp(cmd, "settings") == 0) {
        Serial.printf("I settings: city=%s lat=%.3f lon=%.3f tz=%d thr=%.1f min=%.0f max=%.0f stow=%.0f/%.0f factor=%.2f valid=%d\n", s.cfg->cityId,
                      s.cfg->latDeg, s.cfg->lonDeg, s.cfg->tzOffsetMin, s.cfg->moveThresholdDeg, s.cfg->minDeg, s.cfg->maxDeg,
                      s.cfg->windStowKmh, s.cfg->windReturnKmh, s.cfg->anemometerFactor, settings::isValid(*s.cfg));
    } else if (strncmp(cmd, "set thr ", 8) == 0) {
        settings::Settings next = *s.cfg;
        next.moveThresholdDeg = static_cast<float>(atof(cmd + 8));
        const char* bad = nullptr;
        if (s.store->save(next, &bad)) {
            *s.cfg = next;
            Serial.printf("I set: saved thr=%.1f\n", s.cfg->moveThresholdDeg);
        } else {
            Serial.printf("E set: rejected, invalid field %s\n", bad ? bad : "?");
        }
    } else if (strcmp(cmd, "set reset") == 0) {
        *s.cfg = settings::Settings();
        Serial.printf("I set: defaults %s\n", s.store->save(*s.cfg) ? "saved" : "not saved");
    } else if (strncmp(cmd, "clock set ", 10) == 0) {
        const bool ok = s.clock->setUtc(atoll(cmd + 10));
        s.log->append(hal::EventType::ClockSet, ok ? "bench" : "bench failed");
        Serial.printf("%c clock: set %s\n", ok ? 'I' : 'E', ok ? "ok" : "failed");
    } else if (strcmp(cmd, "clock") == 0) {
        Serial.printf("I clock: present=%d lost_power=%d utc=%lld\n", s.clock->present(), s.clock->lostPower(),
                      static_cast<long long>(s.clock->utc()));
    } else if (strncmp(cmd, "loggen ", 7) == 0) {
        const int n = atoi(cmd + 7);
        const uint32_t t0 = millis();
        for (int i = 0; i < n; ++i) {
            char d[48];
            snprintf(d, sizeof(d), "test event %d with some padding text", i);
            s.log->append(hal::EventType::StateChange, d);
            if ((i & 31) == 0) feedWatchdog();
        }
        s.log->flush();
        Serial.printf("I loggen: %d events in %lu ms, active file %d is %u bytes\n", n, static_cast<unsigned long>(millis() - t0),
                      s.log->activeFileIndex(), static_cast<unsigned>(s.log->activeFileBytes()));
    } else if (strncmp(cmd, "log", 3) == 0) {
        static char out[1800];
        const int n = cmd[3] == ' ' ? atoi(cmd + 4) : 10;
        const size_t len = s.log->readLatest(out, sizeof(out), static_cast<size_t>(n > 0 ? n : 10));
        Serial.printf("I log: %u bytes, active file %d (%u bytes)\n", static_cast<unsigned>(len), s.log->activeFileIndex(),
                      static_cast<unsigned>(s.log->activeFileBytes()));
        Serial.print(out);
    } else if (strncmp(cmd, "ctl", 3) == 0) {
        directMotion = false;
        const char* arg = cmd[3] == ' ' ? cmd + 4 : "";
        control::CommandResult res = control::CommandResult::Ok;
        if (strcmp(arg, "auto") == 0) res = s.ctl->command(control::Command::ModeAuto, s.uptimeMs);
        else if (strcmp(arg, "manual") == 0) res = s.ctl->command(control::Command::ModeManual, s.uptimeMs);
        else if (strcmp(arg, "e") == 0) res = s.ctl->command(control::Command::ManualEast, s.uptimeMs);
        else if (strcmp(arg, "w") == 0) res = s.ctl->command(control::Command::ManualWest, s.uptimeMs);
        else if (strcmp(arg, "stop") == 0) res = s.ctl->command(control::Command::ManualStop, s.uptimeMs);
        else if (strcmp(arg, "ka") == 0) res = s.ctl->command(control::Command::KeepAlive, s.uptimeMs);
        else if (strcmp(arg, "reset") == 0) res = s.ctl->command(control::Command::ResetFault, s.uptimeMs);
        const control::Status st = s.ctl->status(s.uptimeMs);
        Serial.printf("I ctl: state=%s fault=%s target=%.1f angle=%.1f moving=%d text=%d wait_s=%lu result=%d\n", control::stateName(st.state),
                      control::faultName(st.fault), st.targetDeg, st.angleDeg, st.moving, static_cast<int>(st.text),
                      static_cast<unsigned long>(st.waitS), static_cast<int>(res));
    } else if (strncmp(cmd, "windgen ", 8) == 0) {
        genHz = static_cast<uint32_t>(atoi(cmd + 8));
        Serial.printf("I windgen: %lu Hz\n", static_cast<unsigned long>(genHz));
    } else if (strcmp(cmd, "wind") == 0) {
        const wind::Config wc{s.cfg->anemometerFactor, s.cfg->windStowKmh, s.cfg->windStowDelayS, s.cfg->windReturnKmh, s.cfg->windReturnMin};
        Serial.printf("I wind: speed=%.1f stow=%d return_in_s=%lu pulses=%lu factor=%.2f\n", s.wind->speedKmh(), s.wind->stow(),
                      static_cast<unsigned long>(s.wind->returnRemainingS(s.uptimeMs, wc)), static_cast<unsigned long>(s.anemometer->total()),
                      s.cfg->anemometerFactor);
    } else if (strncmp(cmd, "csv", 3) == 0) {
        // The export path of module 012 on real flash data: header and the first `max` bytes, then the total size.
        const size_t max = cmd[3] == ' ' ? static_cast<size_t>(atoi(cmd + 4)) : 600;
        CsvStream stream(*s.log, s.cfg->tzOffsetMin);
        char buf[200];
        size_t total = 0, shown = 0, n;
        Serial.println("I csv begin");
        while ((n = stream.next(buf, sizeof(buf))) > 0) {
            if (shown < max) {
                const size_t w = n < max - shown ? n : max - shown;
                Serial.write(reinterpret_cast<const uint8_t*>(buf), w);
                shown += w;
            }
            total += n;
            feedWatchdog();
        }
        Serial.printf("\nI csv end: total_bytes=%u\n", static_cast<unsigned>(total));
    } else if (strncmp(cmd, "current", 7) == 0) {
        if (strcmp(cmd, "current on") == 0) s.current->begin(s.cfg->currentFactor);
        Serial.printf("I current: started=%d rms_v=%.4f amps=%.3f factor=%.2f module_present_setting=%d\n", s.current->present(),
                      s.current->rmsVolts(), s.current->rmsAmps(), s.cfg->currentFactor, s.cfg->currentPresent);
    } else if (strcmp(cmd, "pins") == 0) {
        printPins();
    } else if (strcmp(cmd, "hang") == 0) {
        Serial.println("W hanging: watchdog must reset the chip within 5 s");
        Serial.flush();
        for (;;) {
        }
    } else if (strcmp(cmd, "reboot") == 0) {
        Serial.println("I rebooting");
        Serial.flush();
        ESP.restart();
    } else if (strcmp(cmd, "help") == 0) {
        Serial.println("I commands: status sun angle cal pins hang reboot help");
    } else if (cmd[0] != '\0') {
        Serial.printf("E unknown command '%s' (try help)\n", cmd);
    }
}

}  // namespace

bool benchDirectMotion() { return directMotion; }

hal::IEventLog* benchEventLog() { return &serialEventLog; }

void benchConsolePoll(const BenchStatus& status) {
    if (genHz > 0 && micros() - genLastUs >= 1000000UL / genHz) {  // one falling edge on the real input pin
        genLastUs = micros();
        pinMode(Anemometer::kPin, OUTPUT);
        digitalWrite(Anemometer::kPin, LOW);
        delayMicroseconds(100);
        pinMode(Anemometer::kPin, INPUT_PULLUP);
    }
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            lineBuf[lineLen] = '\0';
            handle(lineBuf, status);
            lineLen = 0;
        } else if (lineLen < sizeof(lineBuf) - 1) {
            lineBuf[lineLen++] = c;
        }
    }
}

}  // namespace drivers
