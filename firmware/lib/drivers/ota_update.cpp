#include "ota_update.h"

#include <Arduino.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_ota_ops.h>

namespace drivers {

// The text the uploaded image must contain. `used` keeps it in the image although nothing reads it.
extern "C" const char kSunTrackerMagic[] __attribute__((used)) = SUNTRACKER_MAGIC;

namespace {

ota::BootState load() {
    Preferences p;
    ota::BootState s;
    if (p.begin("ota", true)) {
        s.pendingVerify = p.getBool("pending", false);
        s.bootTries = static_cast<uint8_t>(p.getUChar("tries", 0));
        p.end();
    }
    return s;
}

void store(const ota::BootState& s) {
    Preferences p;
    if (p.begin("ota", false)) {
        p.putBool("pending", s.pendingVerify);
        p.putUChar("tries", s.bootTries);
        p.end();
    }
}

}  // namespace

void OtaUpdate::armProbation() {
    ota::BootState s;
    ota::startProbation(s);
    store(s);
}

void OtaUpdate::probationState(bool& pending, uint8_t& tries) {
    const ota::BootState s = load();
    pending = s.pendingVerify;
    tries = s.bootTries;
}

void OtaUpdate::bootCheck(hal::IEventLog* log) {
    volatile const char* keep = kSunTrackerMagic;  // make sure the text is linked in
    (void)keep;
    ota::BootState s = load();
    const ota::BootAction action = ota::onBoot(s);
    if (action == ota::BootAction::Normal) return;
    if (action == ota::BootAction::Probation) {
        store(s);
        confirmed_ = false;
        return;
    }
    // Rollback: the new image did not confirm itself in three starts. Start the previous one.
    const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);  // the slot we are not running from
    ota::confirm(s);
    store(s);
    if (other && esp_ota_set_boot_partition(other) == ESP_OK) {
        // A Fault event is written to flash at once; a StateChange would wait up to 10 s in the buffer and be lost by the restart.
        if (log) log->append(hal::EventType::Fault, "UpdateRolledBack: new firmware did not confirm itself");
        delay(200);
        ESP.restart();
    }
    if (log) log->append(hal::EventType::Fault, "update rollback failed: no valid previous firmware");
}

void OtaUpdate::confirmIfHealthy(uint32_t uptimeMs, bool healthy, hal::IEventLog* log) {
    if (confirmed_ || uptimeMs < kSelfTestMs) return;
    confirmed_ = true;
    ota::BootState s = load();
    if (!s.pendingVerify) return;
    if (!healthy) {
        confirmed_ = false;  // keep waiting; the boot counter keeps counting
        return;
    }
    ota::confirm(s);
    store(s);
    if (log) log->append(hal::EventType::StateChange, "UpdateOk: new firmware confirmed");
}

bool OtaUpdate::fail(const char* textFa, hal::IEventLog* log, const char* why) {
    error_ = textFa;
    if (updating_) Update.abort();
    updating_ = false;
    delete session_;
    session_ = nullptr;
    if (log) log->append(hal::EventType::Fault, why);
    return false;
}

void OtaUpdate::abort() {
    if (updating_) Update.abort();
    updating_ = false;
    delete session_;
    session_ = nullptr;
}

bool OtaUpdate::handleChunk(const uint8_t* data, size_t len, size_t index, bool final, uint32_t nowMs, hal::IEventLog* log) {
    if (index == 0) {
        abort();
        succeeded_ = false;
        error_ = "";
        const esp_partition_t* slot = esp_ota_get_next_update_partition(nullptr);
        if (!slot) return fail("فضای به‌روزرسانی در دسترس نیست", log, "update refused: no OTA slot");
        session_ = new ota::UploadSession(slot->size, nowMs);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) return fail("شروع به‌روزرسانی ناموفق بود", log, "update refused: Update.begin failed");
        updating_ = true;
        if (log) log->append(hal::EventType::StateChange, "UpdateStart");
    }
    if (!updating_ || !session_) return false;

    switch (session_->accept(data, len, nowMs)) {
        case ota::UploadStatus::TooBig: return fail("فایل از فضای نرم‌افزار بزرگ‌تر است", log, "update refused: file too big");
        case ota::UploadStatus::TimedOut: return fail("زمان ارسال فایل تمام شد", log, "update refused: upload took more than 3 minutes");
        default: break;
    }
    if (Update.write(const_cast<uint8_t*>(data), len) != len) return fail("نوشتن در حافظه ناموفق بود", log, "update failed: flash write");

    if (!final) return true;
    if (session_->finish() != ota::UploadStatus::Ok) return fail("فایل نامعتبر است", log, "update refused: not a SunTracker image");
    if (!Update.end(true)) return fail("فایل نامعتبر است", log, "update refused: image check failed");  // ESP-IDF verifies header, size and hash

    armProbation();
    updating_ = false;
    succeeded_ = true;
    doneAtMs_ = nowMs;
    delete session_;
    session_ = nullptr;
    if (log) log->append(hal::EventType::StateChange, "UpdateInstalled: restarting into the new firmware");
    return true;
}

}  // namespace drivers
