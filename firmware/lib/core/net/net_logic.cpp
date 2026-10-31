#include "net_logic.h"

#include <stdio.h>
#include <string.h>

#include "rtc_time.h"
#include "settings.h"

namespace net {

bool NotifyQueue::enqueue(const char* key, const char* text, uint32_t nowMs) {
    // Forget old "seen" entries, then merge duplicates of the last 10 minutes.
    for (size_t i = 0; i < seen_.size();) {
        if (nowMs - seen_[i].atMs > kMergeMs) seen_.erase(seen_.begin() + static_cast<long>(i));
        else ++i;
    }
    for (const Seen& s : seen_)
        if (s.key == key) return false;
    if (items_.size() >= kCapacity) items_.erase(items_.begin());  // the oldest makes room: a new fault matters more
    items_.push_back({key, text});
    seen_.push_back({key, nowMs});
    return true;
}

bool NotifyQueue::peek(uint32_t nowMs, std::string& text) const {
    if (items_.empty()) return false;
    if (everSent_ && nowMs - lastSendMs_ < kMinGapMs) return false;
    text = items_.front().text;
    return true;
}

void NotifyQueue::done(bool sent, uint32_t nowMs) {
    if (sent && !items_.empty()) items_.erase(items_.begin());
    lastSendMs_ = nowMs;  // a failed try also waits the gap before the next one
    everSent_ = true;
}

void tidyScan(std::vector<ScanEntry>& e, size_t maxEntries) {
    std::vector<ScanEntry> out;
    for (const ScanEntry& x : e) {
        if (x.ssid.empty()) continue;  // hidden network
        bool merged = false;
        for (ScanEntry& y : out)
            if (y.ssid == x.ssid) {
                if (x.rssi > y.rssi) y = x;
                merged = true;
                break;
            }
        if (!merged) out.push_back(x);
    }
    for (size_t i = 1; i < out.size(); ++i) {  // insertion sort, strongest first (the list is short)
        ScanEntry key = out[i];
        size_t j = i;
        while (j > 0 && out[j - 1].rssi < key.rssi) {
            out[j] = out[j - 1];
            --j;
        }
        out[j] = key;
    }
    if (out.size() > maxEntries) out.resize(maxEntries);
    e = out;
}

std::string formatNotification(const char* device, const char* textFa, int64_t utcEpoch, int tzOffsetMin) {
    char buf[200];
    if (utcEpoch > 0) {
        const rtc::DateTime l = rtc::fromEpoch(utcEpoch + static_cast<int64_t>(tzOffsetMin) * 60);
        snprintf(buf, sizeof(buf), "%s: %s — %02d:%02d", device, textFa, l.hour, l.minute);
    } else {
        snprintf(buf, sizeof(buf), "%s: %s", device, textFa);
    }
    return buf;
}

bool shouldSetClockFromNtp(int64_t rtcUtc, bool rtcValid, int64_t ntpUtc) {
    if (ntpUtc < 1704067200) return false;  // before 2024: not a real answer
    if (!rtcValid) return true;
    const int64_t diff = ntpUtc > rtcUtc ? ntpUtc - rtcUtc : rtcUtc - ntpUtc;
    return diff > 2;
}

bool validate(const NetConfig& c, const char** badField) {
    auto bad = [&](const char* f) {
        if (badField) *badField = f;
        return false;
    };
    if (memchr(c.ssid, '\0', sizeof(c.ssid)) == nullptr || strlen(c.ssid) > 32) return bad("ssid");
    if (memchr(c.password, '\0', sizeof(c.password)) == nullptr) return bad("password");
    if (c.password[0] != '\0' && strlen(c.password) < 8) return bad("password");  // WPA2 needs at least 8
    if (memchr(c.botToken, '\0', sizeof(c.botToken)) == nullptr) return bad("botToken");
    if (memchr(c.chatId, '\0', sizeof(c.chatId)) == nullptr) return bad("chatId");
    for (const char* p = c.botToken; *p; ++p)
        if (*p == '/' || *p == ' ' || *p == '?' || *p == '#') return bad("botToken");  // it goes into a URL path
    return true;
}

namespace {
constexpr const char* kKey = "netcfg";
struct Blob {
    uint32_t crc;
    NetConfig cfg;
};
}  // namespace

bool save(hal::ISettingsStore& store, const NetConfig& c) {
    if (!validate(c, nullptr)) return false;
    Blob b{settings::crc32(&c, sizeof(c)), c};
    return store.save(kKey, &b, sizeof(b));
}

bool load(hal::ISettingsStore& store, NetConfig& c) {
    Blob b;
    if (store.size(kKey) != sizeof(b) || !store.load(kKey, &b, sizeof(b))) return false;
    if (settings::crc32(&b.cfg, sizeof(b.cfg)) != b.crc || !validate(b.cfg, nullptr)) return false;
    c = b.cfg;
    return true;
}

}  // namespace net
