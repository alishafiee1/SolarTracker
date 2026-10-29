#include "littlefs_event_log.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>

#include "event_format.h"

namespace drivers {

namespace {
const char* pathFor(int index) { return index == 0 ? "/log/events.0.csv" : "/log/events.1.csv"; }
}  // namespace

bool LittleFsEventLog::begin() {
    // The partition is named "littlefs" in partitions.csv (the library default label is "spiffs").
    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) return false;  // true: format on the first mount
    LittleFS.mkdir("/log");
    // The active file is the one that was written last (the larger modification time is not kept
    // by LittleFS, so use the size: the other file is only ever non-empty after a rotation and is
    // then complete, i.e. at least kMaxFileBytes).
    File f0 = LittleFS.open(pathFor(0), "r");
    File f1 = LittleFS.open(pathFor(1), "r");
    const size_t s0 = f0 ? f0.size() : 0;
    const size_t s1 = f1 ? f1.size() : 0;
    if (f0) f0.close();
    if (f1) f1.close();
    if (s0 >= kMaxFileBytes && s1 < kMaxFileBytes) {
        active_ = 1;
        activeBytes_ = s1;
    } else {
        active_ = 0;
        activeBytes_ = s0 >= kMaxFileBytes ? 0 : s0;
        if (s0 >= kMaxFileBytes) {  // both full (power cut during a rotation): start over on file 0
            LittleFS.remove(pathFor(0));
        }
    }
    ready_ = true;
    lastFlushMs_ = time_.nowMs();
    return true;
}

void LittleFsEventLog::append(hal::EventType type, const char* detail) {
    char line[events::kMaxLine];
    const size_t n = events::formatLine(line, sizeof(line), clock_.utc(), type, detail);
    if (n == 0) return;
    if (bufferLen_ + n > kBufferBytes) writeBuffered();
    memcpy(buffer_ + bufferLen_, line, n);
    bufferLen_ += n;
    if (type == hal::EventType::Fault || type == hal::EventType::PowerUp) writeBuffered();  // do not lose these
}

void LittleFsEventLog::service() {
    if (bufferLen_ > 0 && time_.nowMs() - lastFlushMs_ >= kFlushIntervalMs) writeBuffered();
}

void LittleFsEventLog::flush() { writeBuffered(); }

void LittleFsEventLog::rotateIfNeeded() {
    if (activeBytes_ < kMaxFileBytes) return;
    active_ = 1 - active_;
    LittleFS.remove(pathFor(active_));  // the older file is dropped
    activeBytes_ = 0;
}

void LittleFsEventLog::writeBuffered() {
    lastFlushMs_ = time_.nowMs();
    if (!ready_ || bufferLen_ == 0) {
        bufferLen_ = 0;
        return;
    }
    rotateIfNeeded();
    File f = LittleFS.open(pathFor(active_), "a");
    if (f) {
        f.write(reinterpret_cast<const uint8_t*>(buffer_), bufferLen_);
        activeBytes_ += bufferLen_;
        f.close();
    }
    bufferLen_ = 0;
}

size_t LittleFsEventLog::readLatest(char* out, size_t outLen, size_t max) {
    writeBuffered();
    if (!ready_ || outLen == 0) return 0;
    // Collect from the active file, then the older one, keeping the last `max` lines.
    size_t written = 0;
    out[0] = '\0';
    const int order[2] = {1 - active_, active_};  // oldest file first
    // Count lines first so the newest `max` are kept.
    size_t total = 0;
    for (int idx : order) {
        File f = LittleFS.open(pathFor(idx), "r");
        if (!f) continue;
        uint8_t block[256];
        int got;
        while ((got = f.read(block, sizeof(block))) > 0) {  // block reads: byte-wise reads of 64 KB take seconds
            for (int i = 0; i < got; ++i)
                if (block[i] == '\n') ++total;
        }
        f.close();
    }
    size_t skip = total > max ? total - max : 0;
    for (int idx : order) {
        File f = LittleFS.open(pathFor(idx), "r");
        if (!f) continue;
        char line[events::kMaxLine];
        while (f.available()) {
            const size_t n = f.readBytesUntil('\n', line, sizeof(line) - 2);
            line[n] = '\n';
            line[n + 1] = '\0';
            if (skip > 0) {
                --skip;
                continue;
            }
            if (written + n + 2 > outLen) break;
            memcpy(out + written, line, n + 1);
            written += n + 1;
            out[written] = '\0';
        }
        f.close();
    }
    return written;
}

}  // namespace drivers
