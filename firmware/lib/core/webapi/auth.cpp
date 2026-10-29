#include "auth.h"

#include <string.h>

#include "sha256.h"

namespace webapi {

namespace {
// Compare without stopping at the first difference.
bool equalConstTime(const uint8_t* a, const uint8_t* b, size_t n) {
    uint8_t diff = 0;
    for (size_t i = 0; i < n; ++i) diff |= a[i] ^ b[i];
    return diff == 0;
}

bool equalStrConstTime(const char* a, const char* b) {
    const size_t la = strlen(a), lb = strlen(b);
    uint8_t diff = static_cast<uint8_t>(la != lb);
    const size_t n = la < lb ? la : lb;
    for (size_t i = 0; i < n; ++i) diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    return diff == 0;
}

void toHex(const uint8_t* in, size_t n, char* out) {
    static const char* d = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) {
        out[i * 2] = d[in[i] >> 4];
        out[i * 2 + 1] = d[in[i] & 15];
    }
    out[n * 2] = '\0';
}
}  // namespace

bool Auth::verify(const settings::Settings& cfg, const char* defaultPassword, const char* password) const {
    if (!password) return false;
    if (cfg.passwordIsDefault) return defaultPassword && defaultPassword[0] != '\0' && equalStrConstTime(password, defaultPassword);
    uint8_t h[32];
    saltedHash(cfg.settingsPasswordSalt, sizeof(cfg.settingsPasswordSalt), password, h);
    return equalConstTime(h, cfg.settingsPasswordHash, 32);
}

LoginResult Auth::login(const settings::Settings& cfg, const char* defaultPassword, const char* password, uint32_t nowMs, std::string& tokenOut) {
    if (locked_) {
        if (static_cast<int32_t>(nowMs - lockedUntilMs_) < 0) return LoginResult::Locked;
        locked_ = false;
        failures_ = 0;
    }
    if (!verify(cfg, defaultPassword, password)) {
        if (failures_ == 0 || nowMs - firstFailureMs_ > kFailureWindowMs) {
            failures_ = 0;
            firstFailureMs_ = nowMs;
        }
        if (++failures_ >= kMaxFailures) {
            locked_ = true;
            lockedUntilMs_ = nowMs + kLockMs;
        }
        return LoginResult::WrongPassword;
    }
    failures_ = 0;

    // Free slot, else drop the session that has been idle the longest.
    Session* slot = nullptr;
    for (Session& s : sessions_)
        if (!s.used) {
            slot = &s;
            break;
        }
    if (!slot) {
        slot = &sessions_[0];
        for (Session& s : sessions_)
            if (static_cast<int32_t>(slot->lastMs - s.lastMs) > 0) slot = &s;
    }
    uint8_t raw[16];
    rng_.bytes(raw, sizeof(raw));  // 128 bits
    toHex(raw, sizeof(raw), slot->token);
    slot->used = true;
    slot->lastMs = nowMs;
    tokenOut = slot->token;
    return LoginResult::Ok;
}

bool Auth::check(const char* token, uint32_t nowMs) {
    if (!token || strlen(token) != 32) return false;
    for (Session& s : sessions_) {
        if (!s.used || strcmp(s.token, token) != 0) continue;
        if (nowMs - s.lastMs > kSessionIdleMs) {
            s.used = false;
            return false;
        }
        s.lastMs = nowMs;
        return true;
    }
    return false;
}

void Auth::logout(const char* token) {
    if (!token) return;
    for (Session& s : sessions_)
        if (s.used && strcmp(s.token, token) == 0) s.used = false;
}

bool Auth::setPassword(settings::Settings& cfg, const char* newPassword) {
    if (!newPassword || strlen(newPassword) < kMinPasswordLen || strlen(newPassword) > 64) return false;
    rng_.bytes(cfg.settingsPasswordSalt, sizeof(cfg.settingsPasswordSalt));
    saltedHash(cfg.settingsPasswordSalt, sizeof(cfg.settingsPasswordSalt), newPassword, cfg.settingsPasswordHash);
    cfg.passwordIsDefault = false;
    for (Session& s : sessions_) s.used = false;
    return true;
}

size_t Auth::sessionCount() const {
    size_t n = 0;
    for (const Session& s : sessions_) n += s.used;
    return n;
}

}  // namespace webapi
