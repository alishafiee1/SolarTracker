#pragma once
#include <FS.h>
#include <hal.h>

namespace drivers {

// Rotating CSV event log on LittleFS (cycle 007, R6): /log/events.0.csv and /log/events.1.csv,
// the active one rotates at 64 KB. Lines are buffered and written at most every 10 s, or at once
// for faults, to limit flash wear. Timestamps come from the clock given to the constructor.
class LittleFsEventLog : public hal::IEventLog {
public:
    static constexpr size_t kMaxFileBytes = 64 * 1024;
    static constexpr uint32_t kFlushIntervalMs = 10000;

    LittleFsEventLog(const hal::IClock& clock, const hal::ITime& time) : clock_(clock), time_(time) {}

    // Mount (formatting once if needed) and find the active file. Returns false if the flash is unusable.
    bool begin();
    void append(hal::EventType type, const char* detail) override;
    // Call from the main loop: writes buffered lines when the interval has passed.
    void service();
    void flush();

    // The newest `max` lines (newest last) into out as "line\n"; returns the byte count.
    size_t readLatest(char* out, size_t outLen, size_t max);
    // Reading the whole log oldest line first (CSV export). One reader at a time; appends are buffered meanwhile.
    void beginRead();
    bool readLine(char* out, size_t len);  // false at the end
    void endRead();
    size_t activeFileBytes() const { return activeBytes_; }
    // Bytes in both log files (the log keeps at most 2 x kMaxFileBytes, the oldest file is dropped at a rotation).
    size_t storedBytes();
    int activeFileIndex() const { return active_; }

private:
    static constexpr size_t kBufferBytes = 1024;
    void writeBuffered();
    void rotateIfNeeded();

    const hal::IClock& clock_;
    const hal::ITime& time_;
    bool ready_ = false;
    int active_ = 0;
    size_t activeBytes_ = 0;
    char buffer_[kBufferBytes];
    size_t bufferLen_ = 0;
    uint32_t lastFlushMs_ = 0;
    int readStep_ = 0;  // 0 = oldest file, 1 = newest file, 2 = done
    bool reading_ = false;
    File readFile_;
};

}  // namespace drivers
