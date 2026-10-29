#include "event_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace events {

const char* typeName(hal::EventType type) {
    switch (type) {
        case hal::EventType::PowerUp: return "PowerUp";
        case hal::EventType::StateChange: return "StateChange";
        case hal::EventType::Fault: return "Fault";
        case hal::EventType::FaultReset: return "FaultReset";
        case hal::EventType::MoveStart: return "MoveStart";
        case hal::EventType::MoveEnd: return "MoveEnd";
        case hal::EventType::ClockSet: return "ClockSet";
        case hal::EventType::SettingsChange: return "SettingsChange";
        case hal::EventType::SettingsReset: return "SettingsReset";
        case hal::EventType::SettingsMigrated: return "SettingsMigrated";
        case hal::EventType::SupplyLow: return "SupplyLow";
        case hal::EventType::LogReset: return "LogReset";
    }
    return "Unknown";
}

size_t formatLine(char* buf, size_t len, int64_t utcEpoch, hal::EventType type, const char* detail) {
    char clean[kMaxDetail + 1];
    size_t n = 0;
    for (const char* p = detail ? detail : ""; *p != '\0' && n < kMaxDetail; ++p) {
        char c = *p;
        if (c == ',') c = ';';
        if (c == '\n' || c == '\r') c = ' ';
        clean[n++] = c;
    }
    clean[n] = '\0';
    const int written = snprintf(buf, len, "%lld,%s,%s\n", static_cast<long long>(utcEpoch), typeName(type), clean);
    if (written < 0 || static_cast<size_t>(written) >= len) return 0;
    return static_cast<size_t>(written);
}

bool parseLine(const char* line, int64_t& utcEpoch, char* typeOut, size_t typeLen, char* detailOut, size_t detailLen) {
    char* end = nullptr;
    const long long epoch = strtoll(line, &end, 10);
    if (end == line || *end != ',') return false;
    const char* typeStart = end + 1;
    const char* comma = strchr(typeStart, ',');
    if (!comma) return false;
    const size_t tl = static_cast<size_t>(comma - typeStart);
    if (tl == 0 || tl >= typeLen) return false;
    memcpy(typeOut, typeStart, tl);
    typeOut[tl] = '\0';
    const char* d = comma + 1;
    size_t dl = strcspn(d, "\r\n");
    if (dl >= detailLen) dl = detailLen - 1;
    memcpy(detailOut, d, dl);
    detailOut[dl] = '\0';
    utcEpoch = epoch;
    return true;
}

}  // namespace events
