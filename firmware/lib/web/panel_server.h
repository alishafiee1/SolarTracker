#pragma once
// Hotspot and HTTP server of the web panel (cycle 008): turns HTTP requests into webapi::Request, serves the
// embedded panel files (gzip) and answers captive-portal probes. All logic lives in webapi::WebApi.

#include <stdint.h>

#include <functional>

#include "littlefs_event_log.h"
#include "web_api.h"

namespace web {

class PanelServer {
public:
    explicit PanelServer(webapi::WebApi& api) : api_(api) {}

    // Starts the access point (WPA2, 192.168.4.1, max 4 clients, channel 6) and the web server.
    // `apPassword` must have at least 8 characters.
    bool begin(const char* apPassword);

    // Handlers run in the network task. The control loop takes the same lock around a tick, so the
    // API never sees half-updated state (R7). Handlers are short (< 20 ms apart from flash writes).
    void lock();
    void unlock();

    const char* ssid() const { return ssid_; }
    uint8_t clients() const;

    // CSV download of the event log (module 012). Set before begin(); both are optional.
    void setLogCsv(drivers::LittleFsEventLog* log, std::function<int()> tzOffsetMin, std::function<int64_t()> utcNow);
    drivers::LittleFsEventLog* csvLog() const { return csvLog_; }
    int tzOffsetMin() const { return tz_ ? tz_() : 0; }
    int64_t utcNow() const { return utc_ ? utc_() : 0; }
    uint32_t requestsServed() const { return served_; }
    webapi::WebApi& api() { return api_; }
    void countServed() { ++served_; }

private:
    webapi::WebApi& api_;
    char ssid_[24] = {};
    void* mutex_ = nullptr;
    uint32_t served_ = 0;
    drivers::LittleFsEventLog* csvLog_ = nullptr;
    std::function<int()> tz_;
    std::function<int64_t()> utc_;
};

}  // namespace web
