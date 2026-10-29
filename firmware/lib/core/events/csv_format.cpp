#include "csv_format.h"

#include <stdio.h>
#include <string.h>

#include "rtc_time.h"

namespace events {

namespace {

// Appends one CSV field; returns false when the buffer is full.
bool putField(char* buf, size_t len, size_t& pos, const char* text) {
    bool quote = text[0] == ' ';
    for (const char* p = text; *p && !quote; ++p) quote = *p == ',' || *p == '"' || *p == '\r' || *p == '\n';
    auto put = [&](char c) {
        if (pos + 1 >= len) return false;
        buf[pos++] = c;
        return true;
    };
    if (quote && !put('"')) return false;
    for (const char* p = text; *p; ++p) {
        if (*p == '"' && !put('"')) return false;
        if (!put(*p)) return false;
    }
    return !quote || put('"');
}

}  // namespace

size_t csvHeader(char* buf, size_t len) {
    static const char kHeader[] = "\xEF\xBB\xBF" "local_time,utc_time,type,detail\r\n";
    if (len < sizeof(kHeader)) return 0;
    memcpy(buf, kHeader, sizeof(kHeader));
    return sizeof(kHeader) - 1;
}

size_t csvRow(char* buf, size_t len, int64_t utcEpoch, int tzOffsetMin, const char* type, const char* detail) {
    char local[40] = "";
    char utc[24] = "";
    if (utcEpoch > 0) {
        const rtc::DateTime u = rtc::fromEpoch(utcEpoch);
        snprintf(utc, sizeof(utc), "%04d-%02d-%02dT%02d:%02d:%02dZ", u.year, u.month, u.day, u.hour, u.minute, u.second);
        const rtc::DateTime l = rtc::fromEpoch(utcEpoch + static_cast<int64_t>(tzOffsetMin) * 60);
        const int off = tzOffsetMin < 0 ? -tzOffsetMin : tzOffsetMin;
        snprintf(local, sizeof(local), "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d", l.year, l.month, l.day, l.hour, l.minute, l.second,
                 tzOffsetMin < 0 ? '-' : '+', off / 60, off % 60);
    }
    size_t pos = 0;
    if (!putField(buf, len, pos, local) || pos + 1 >= len) return 0;
    buf[pos++] = ',';
    if (!putField(buf, len, pos, utc) || pos + 1 >= len) return 0;
    buf[pos++] = ',';
    if (!putField(buf, len, pos, type ? type : "") || pos + 1 >= len) return 0;
    buf[pos++] = ',';
    if (!putField(buf, len, pos, detail ? detail : "") || pos + 2 >= len) return 0;
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return pos;
}

}  // namespace events
