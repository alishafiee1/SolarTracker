#include "bench_console.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "relays_gpio.h"
#include "sun_position.h"
#include "tracking.h"

namespace drivers {

namespace {

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
        Serial.println("I commands: status pins hang reboot help");
    } else if (cmd[0] != '\0') {
        Serial.printf("E unknown command '%s' (try help)\n", cmd);
    }
}

}  // namespace

void benchConsolePoll(const BenchStatus& status) {
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
