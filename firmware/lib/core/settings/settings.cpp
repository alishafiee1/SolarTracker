#include "settings.h"

#include <math.h>
#include <string.h>
#include <type_traits>

namespace settings {

static_assert(std::is_trivially_copyable<Settings>::value, "Settings is stored as raw bytes");

namespace {

constexpr uint32_t kMagic = 0x53544E47;  // "STNG"
constexpr const char* kKey = "settings";

struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t payloadSize;
    uint32_t crc;  // over the payload
};

bool inRange(float v, float lo, float hi) { return isfinite(v) && v >= lo && v <= hi; }
bool inRange(uint32_t v, uint32_t lo, uint32_t hi) { return v >= lo && v <= hi; }

}  // namespace

uint32_t crc32(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

size_t validate(const Settings& s, const char** invalid, size_t max) {
    size_t n = 0;
    auto bad = [&](const char* name) {
        if (n < max) invalid[n] = name;
        ++n;
    };
    if (!inRange(s.latDeg, -90.0f, 90.0f)) bad("latDeg");
    if (!inRange(s.lonDeg, -180.0f, 180.0f)) bad("lonDeg");
    if (memchr(s.cityId, '\0', sizeof(s.cityId)) == nullptr) bad("cityId");
    if (s.tzOffsetMin < -720 || s.tzOffsetMin > 840) bad("tzOffsetMin");
    if (!inRange(s.minDeg, -60.0f, 0.0f)) bad("minDeg");
    if (!inRange(s.maxDeg, 0.0f, 60.0f)) bad("maxDeg");
    if (!(s.minDeg < s.maxDeg)) bad("minDeg<maxDeg");
    if (!inRange(s.moveThresholdDeg, 2.0f, 15.0f)) bad("moveThresholdDeg");
    if (!inRange(s.stallCheckMs, 2000u, 20000u)) bad("stallCheckMs");
    if (!inRange(s.preDawnMinutes, 0u, 120u)) bad("preDawnMinutes");
    if (!inRange(s.stopSettleMs, 1000u, 10000u)) bad("stopSettleMs");
    if (!inRange(s.minMoveGapMs, 1000u, 60000u)) bad("minMoveGapMs");
    if (!inRange(s.maxMoveMs, 10000u, 300000u)) bad("maxMoveMs");
    if (!inRange(s.windStowKmh, 10.0f, 100.0f)) bad("windStowKmh");
    if (!inRange(s.windStowDelayS, 1u, 60u)) bad("windStowDelayS");
    if (!inRange(s.windReturnKmh, 5.0f, 100.0f) || !(s.windReturnKmh < s.windStowKmh)) bad("windReturnKmh");
    if (!inRange(s.windReturnMin, 1u, 120u)) bad("windReturnMin");
    if (!inRange(s.anemometerFactor, 1.0f, 20.0f)) bad("anemometerFactor");
    if (s.mode != Mode::Auto && s.mode != Mode::Manual) bad("mode");
    if (!inRange(s.supplyLowV, 3.0f, 6.0f)) bad("supplyLowV");
    if (memchr(s.apPassword, '\0', sizeof(s.apPassword)) == nullptr) bad("apPassword");
    if (!inRange(s.currentFactor, 0.1f, 50.0f)) bad("currentFactor");
    if (!inRange(s.stallCurrentA, 0.5f, 20.0f)) bad("stallCurrentA");
    if (!inRange(s.maxCurrentA, 1.0f, 30.0f) || !(s.stallCurrentA < s.maxCurrentA)) bad("maxCurrentA");
    if (!inRange(s.zeroCurrentA, 0.05f, 1.0f)) bad("zeroCurrentA");
    return n < max ? n : max;
}

LoadResult SettingsStore::load(Settings& out, hal::IEventLog* log) {
    out = Settings();
    const size_t stored = store_.size(kKey);
    uint8_t buf[sizeof(Header) + sizeof(Settings)];
    auto reset = [&](const char* why) {
        out = Settings();
        if (log) log->append(hal::EventType::SettingsReset, why);
        return LoadResult::Reset;
    };

    if (stored == 0) return reset("no stored settings");
    if (stored < sizeof(Header) || stored > sizeof(buf)) return reset("bad size");
    if (!store_.load(kKey, buf, stored)) return reset("read failed");

    Header h;
    memcpy(&h, buf, sizeof(h));
    if (h.magic != kMagic || stored != sizeof(Header) + h.payloadSize) return reset("bad header");
    if (crc32(buf + sizeof(Header), h.payloadSize) != h.crc) return reset("crc mismatch");
    if (h.version > kSettingsVersion) return reset("newer layout");

    // Older (shorter) layouts keep their values; fields appended since stay at their defaults.
    const size_t copy = h.payloadSize < sizeof(Settings) ? h.payloadSize : sizeof(Settings);
    memcpy(&out, buf + sizeof(Header), copy);
    if (!isValid(out)) return reset("invalid values");

    if (h.version < kSettingsVersion || h.payloadSize < sizeof(Settings)) {
        if (log) log->append(hal::EventType::SettingsMigrated, "older layout");
        return LoadResult::Migrated;
    }
    return LoadResult::Loaded;
}

bool SettingsStore::save(const Settings& s, const char** badField) {
    const char* invalid[1];
    if (validate(s, invalid, 1) > 0) {
        if (badField) *badField = invalid[0];
        return false;
    }
    uint8_t buf[sizeof(Header) + sizeof(Settings)];
    Header h{kMagic, kSettingsVersion, static_cast<uint16_t>(sizeof(Settings)), crc32(&s, sizeof(Settings))};
    memcpy(buf, &h, sizeof(h));
    memcpy(buf + sizeof(Header), &s, sizeof(Settings));
    return store_.save(kKey, buf, sizeof(buf));
}

}  // namespace settings
