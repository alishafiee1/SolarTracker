#pragma once
// Offline firmware update from the web panel (cycle 010). The upload goes to the inactive OTA slot; the
// ESP image header, size and appended SHA-256 are checked by the ESP-IDF when the write ends, the project
// text by UploadSession. The new image then runs "on probation" (boot counter in NVS) and rolls back to the
// previous slot if it does not confirm itself.
#include <hal.h>
#include <stdint.h>

#include "ota_logic.h"

namespace drivers {

class OtaUpdate {
public:
    static constexpr uint32_t kSelfTestMs = 60000;  // the new image must run this long before it is confirmed
    static constexpr uint32_t kRestartDelayMs = 1500;

    // Call early in setup() (NVS must work). Counts a probation boot and, after too many, switches back to the
    // previous slot and restarts. Logs UpdateOk / UpdateRolledBack events.
    void bootCheck(hal::IEventLog* log);
    // Call regularly. After kSelfTestMs of uptime with `healthy` true the image is confirmed.
    void confirmIfHealthy(uint32_t uptimeMs, bool healthy, hal::IEventLog* log);

    // Upload, called from the web server's upload callback in order. `final` marks the last chunk.
    // Returns false when the upload was refused or failed; errorText() then says why (Persian).
    bool handleChunk(const uint8_t* data, size_t len, size_t index, bool final, uint32_t nowMs, hal::IEventLog* log);
    void abort();
    // Puts the running image on probation as if it had just been installed (used after an upload and by the bench test).
    static void armProbation();
    // Probation state for the bench console: pending flag and boot count.
    static void probationState(bool& pending, uint8_t& tries);

    bool updating() const { return updating_; }
    bool succeeded() const { return succeeded_; }
    const char* errorText() const { return error_; }
    // After a good update the device restarts once the response has been sent.
    bool restartDue(uint32_t nowMs) const { return succeeded_ && nowMs - doneAtMs_ >= kRestartDelayMs; }

private:
    bool fail(const char* textFa, hal::IEventLog* log, const char* why);

    bool updating_ = false;
    bool succeeded_ = false;
    bool confirmed_ = false;
    const char* error_ = "";
    uint32_t doneAtMs_ = 0;
    ota::UploadSession* session_ = nullptr;
};

}  // namespace drivers
