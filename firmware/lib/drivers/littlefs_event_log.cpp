#include "littlefs_event_log.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <string.h>

#include <string>

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

void LittleFsEventLog::beginRead() {
    writeBuffered();
    readStep_ = 0;
    reading_ = true;
    readFile_ = File();
}

void LittleFsEventLog::endRead() {
    if (readFile_) readFile_.close();
    reading_ = false;
}

bool LittleFsEventLog::readLine(char* out, size_t len) {
    while (reading_ && readStep_ < 2) {
        if (!readFile_) {
            readFile_ = LittleFS.open(pathFor(readStep_ == 0 ? 1 - active_ : active_), "r");
            if (!readFile_) {
                ++readStep_;
                continue;
            }
        }
        if (readFile_.available()) {
            const size_t n = readFile_.readBytesUntil('\n', out, len - 2);
            out[n] = '\n';
            out[n + 1] = '\0';
            return true;
        }
        readFile_.close();
        readFile_ = File();
        ++readStep_;
    }
    return false;
}

namespace {
// The last `bytes` bytes of a file, starting at a line boundary.
std::string tailOf(const char* path, size_t bytes) {
    File f = LittleFS.open(path, "r");
    if (!f) return "";
    const size_t size = f.size();
    size_t start = size > bytes ? size - bytes : 0;
    if (start > 0) f.seek(start);
    std::string text;
    text.resize(size - start);
    const size_t got = f.readBytes(&text[0], size - start);
    text.resize(got);
    f.close();
    if (start > 0) {  // drop the partial first line
        const size_t nl = text.find('\n');
        text = nl == std::string::npos ? "" : text.substr(nl + 1);
    }
    return text;
}
}  // namespace

size_t LittleFsEventLog::readLatest(char* out, size_t outLen, size_t max) {
    writeBuffered();
    if (!ready_ || outLen == 0) return 0;
    out[0] = '\0';
    // Lines are at most kMaxLine bytes, usually about 60: a tail of max * 100 bytes holds the newest `max` lines
    // in all but extreme cases. Only the tail is read, so this stays fast however large the log is.
    size_t want = max * 100 + 200;
    if (want > 16384) want = 16384;
    std::string text = tailOf(pathFor(active_), want);
    size_t lines = 0;
    for (char c : text) lines += c == '\n';
    if (lines < max) text = tailOf(pathFor(1 - active_), want) + text;  // the older file continues before the active one
    // Keep the newest `max` lines.
    size_t keepFrom = text.size();
    size_t seen = 0;
    while (keepFrom > 0 && seen <= max) {
        --keepFrom;
        if (text[keepFrom] == '\n' && keepFrom + 1 < text.size() && ++seen > max) {
            ++keepFrom;
            break;
        }
    }
    if (seen <= max) keepFrom = 0;
    size_t n = text.size() - keepFrom;
    if (n + 1 > outLen) {  // too small: keep the newest part, from a line start
        size_t from = text.size() - (outLen - 1);
        const size_t nl = text.find('\n', from);
        from = nl == std::string::npos ? text.size() : nl + 1;
        keepFrom = from;
        n = text.size() - keepFrom;
    }
    memcpy(out, text.data() + keepFrom, n);
    out[n] = '\0';
    return n;
}

}  // namespace drivers
