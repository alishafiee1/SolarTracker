#pragma once
// Site Wi-Fi (station next to the hotspot), NTP time and Bale notifications (cycle 011). All of it is optional
// and never blocks the control loop: the connection is managed from the main loop with a backoff, and the HTTPS
// requests run in their own low-priority task.
#include <hal.h>
#include <stdint.h>

#include <string>

#include "net_logic.h"

namespace drivers {

class SiteNet {
public:
    // Take (new) configuration; reconnects with the new credentials.
    void apply(const net::NetConfig& cfg);
    // Call from the main loop (cheap). Reconnects with backoff, runs NTP, reports a corrected time through `rtc`.
    void service(uint32_t nowMs, hal::IClock& rtc, bool rtcValid, hal::IEventLog* log, bool setClockFromNtp = true);

    bool connected() const { return connected_; }
    const char* ip() const { return ip_; }
    int lastNotify() const { return lastNotify_; }  // -1 never tried, 0 failed, 1 sent

    // Queue a message (merged and rate limited by net::NotifyQueue). Returns false when notifications are off.
    bool notify(const std::string& key, const std::string& text, uint32_t nowMs);

private:
    static void task(void* self);
    void sendLoop();
    bool post(const std::string& text);

    net::NetConfig cfg_;
    net::Backoff backoff_;
    uint32_t nextTryMs_ = 0;
    bool wanted_ = false;
    bool connected_ = false;
    char ip_[16] = "";
    bool ntpStarted_ = false;
    uint32_t lastNtpMs_ = 0;
    int lastNotify_ = -1;
    bool taskStarted_ = false;
    void* mutex_ = nullptr;
    net::NotifyQueue queue_;
};

}  // namespace drivers
