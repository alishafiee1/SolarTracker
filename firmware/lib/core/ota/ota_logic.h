#pragma once
// Pure logic of the offline firmware update (cycle 010): upload limits, the project magic string and the
// boot-counter rollback rule. The flash writing itself is in lib/drivers/ota_update.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace ota {

// The image must contain this text (it is compiled into every SunTracker firmware); an image built for
// something else is refused even when it is a valid ESP32 image (R3).
#define SUNTRACKER_MAGIC "SUNTRACKER-FW"
constexpr const char kMagic[] = SUNTRACKER_MAGIC;

// Finds a fixed text in a byte stream that arrives in arbitrary chunks.
class MagicScanner {
public:
    explicit MagicScanner(const char* pattern = kMagic) : pattern_(pattern), len_(strlen(pattern)) {}
    void feed(const uint8_t* data, size_t n) {
        for (size_t i = 0; i < n && !found_; ++i) {
            // Partial match state with a restart on mismatch (the pattern has no self-overlap that matters here
            // beyond its first letter, so a simple restart check is enough).
            if (static_cast<char>(data[i]) == pattern_[matched_]) {
                if (++matched_ == len_) found_ = true;
            } else {
                matched_ = static_cast<char>(data[i]) == pattern_[0] ? 1 : 0;
            }
        }
    }
    bool found() const { return found_; }

private:
    const char* pattern_;
    size_t len_;
    size_t matched_ = 0;
    bool found_ = false;
};

enum class UploadStatus : uint8_t { Ok, TooBig, TimedOut, Empty, NoMagic };

// One upload: size limit, the 3 minute time limit and the magic check.
class UploadSession {
public:
    static constexpr uint32_t kMaxDurationMs = 3 * 60 * 1000;

    UploadSession(size_t maxBytes, uint32_t nowMs) : max_(maxBytes), startMs_(nowMs) {}

    UploadStatus accept(const uint8_t* data, size_t n, uint32_t nowMs) {
        if (nowMs - startMs_ > kMaxDurationMs) return UploadStatus::TimedOut;
        if (received_ + n > max_) return UploadStatus::TooBig;
        scanner_.feed(data, n);
        received_ += n;
        return UploadStatus::Ok;
    }
    // Call when the last chunk has been accepted.
    UploadStatus finish() const {
        if (received_ == 0) return UploadStatus::Empty;
        return scanner_.found() ? UploadStatus::Ok : UploadStatus::NoMagic;
    }
    size_t received() const { return received_; }

private:
    size_t max_;
    uint32_t startMs_;
    size_t received_ = 0;
    MagicScanner scanner_;
};

// Boot counter rule (R4). After an update the new image runs "on probation": every boot counts, and the image
// is confirmed once it has run its self-test. The third boot that is still on probation rolls back.
constexpr uint8_t kMaxProbationBoots = 3;

struct BootState {
    bool pendingVerify = false;
    uint8_t bootTries = 0;
};

enum class BootAction : uint8_t { Normal, Probation, Rollback };

inline BootAction onBoot(BootState& s) {
    if (!s.pendingVerify) return BootAction::Normal;
    if (s.bootTries >= kMaxProbationBoots) return BootAction::Rollback;
    ++s.bootTries;
    return BootAction::Probation;
}

inline void confirm(BootState& s) {
    s.pendingVerify = false;
    s.bootTries = 0;
}

inline void startProbation(BootState& s) {
    s.pendingVerify = true;
    s.bootTries = 0;
}

}  // namespace ota
