#pragma once
// Pure logic of the site Wi-Fi and Bale notification module (cycle 011): reconnect backoff, the message queue with
// rate limit and duplicate merging, the notification text, the NTP rule and the stored network configuration.
#include <hal.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace net {

// R1: 5 s, 10 s, 20 s ... up to 5 minutes between connection attempts.
class Backoff {
public:
    static constexpr uint32_t kFirstMs = 5000;
    static constexpr uint32_t kMaxMs = 5 * 60 * 1000;
    uint32_t nextDelayMs() {
        const uint32_t d = delay_;
        delay_ = delay_ * 2 > kMaxMs ? kMaxMs : delay_ * 2;
        return d;
    }
    void reset() { delay_ = kFirstMs; }

private:
    uint32_t delay_ = kFirstMs;
};

// R3: up to 20 messages are kept while offline, at most one is sent per 10 s, and the same fault within 10 minutes is merged.
class NotifyQueue {
public:
    static constexpr size_t kCapacity = 20;
    static constexpr uint32_t kMinGapMs = 10000;
    static constexpr uint32_t kMergeMs = 10 * 60 * 1000;

    // `key` identifies the kind of message ("fault:stall"). Returns false when it was merged or dropped.
    bool enqueue(const char* key, const char* text, uint32_t nowMs);
    // The oldest message that may be sent now (the gap since the last send has passed), else false.
    bool peek(uint32_t nowMs, std::string& text) const;
    // Report the outcome of the send of the message returned by peek(). A failed send keeps it for a retry.
    void done(bool sent, uint32_t nowMs);
    // The cause is gone (e.g. the fault was reset): the same key may be reported again at once.
    void forget(const char* key);
    size_t size() const { return items_.size(); }

private:
    struct Item {
        std::string key, text;
    };
    std::vector<Item> items_;
    struct Seen {
        std::string key;
        uint32_t atMs;
    };
    std::vector<Seen> seen_;
    uint32_t lastSendMs_ = 0;
    bool everSent_ = false;
};

// R3: "<device>: <Persian text> — <local time hh:mm>". An unset clock (epoch 0) leaves the time out.
std::string formatNotification(const char* device, const char* textFa, int64_t utcEpoch, int tzOffsetMin);

// Wi-Fi scan (cycle 015): one entry per SSID (the strongest), no hidden networks, strongest first, at most 20.
struct ScanEntry {
    std::string ssid;
    int rssi;
    bool secure;
};
void tidyScan(std::vector<ScanEntry>& entries, size_t maxEntries = 20);

// R2: the clock is set from NTP only when the time is plausible and differs by more than 2 s.
bool shouldSetClockFromNtp(int64_t rtcUtc, bool rtcValid, int64_t ntpUtc);

// Stored network configuration (kept in its own NVS blob so the secrets never appear in Settings or its API).
struct NetConfig {
    char ssid[33] = "";
    char password[65] = "";
    char botToken[64] = "";
    char chatId[24] = "";
    bool notifyEnabled = false;
};

bool validate(const NetConfig& c, const char** badField);
bool save(hal::ISettingsStore& store, const NetConfig& c);
bool load(hal::ISettingsStore& store, NetConfig& c);

}  // namespace net
