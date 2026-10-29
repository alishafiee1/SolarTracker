#pragma once
// Settings-password check, sessions and login throttling for the web panel (cycle 008, R4).
#include <hal.h>
#include <stdint.h>

#include <string>

#include "settings.h"

namespace webapi {

constexpr uint32_t kSessionIdleMs = 30 * 60 * 1000;  // 30 minutes without a request
constexpr size_t kMaxSessions = 4;
constexpr int kMaxFailures = 5;                       // wrong passwords ...
constexpr uint32_t kFailureWindowMs = 60 * 1000;      // ... within one minute lock logins ...
constexpr uint32_t kLockMs = 60 * 1000;               // ... for one minute
constexpr size_t kMinPasswordLen = 8;

enum class LoginResult : uint8_t { Ok, WrongPassword, Locked };

class Auth {
public:
    explicit Auth(hal::IRandom& rng) : rng_(rng) {}

    // While the stored password is still the shipped default (cfg.passwordIsDefault) it is compared with
    // `defaultPassword`; afterwards with the salted hash in the settings.
    LoginResult login(const settings::Settings& cfg, const char* defaultPassword, const char* password, uint32_t nowMs, std::string& tokenOut);

    // True for a live session; refreshes its idle timer.
    bool check(const char* token, uint32_t nowMs);
    void logout(const char* token);

    // Verify a password without creating a session (for "change password").
    bool verify(const settings::Settings& cfg, const char* defaultPassword, const char* password) const;
    // New salt and hash; refuses passwords shorter than kMinPasswordLen. Ends all sessions.
    bool setPassword(settings::Settings& cfg, const char* newPassword);

    size_t sessionCount() const;

private:
    struct Session {
        bool used = false;
        char token[33] = {};
        uint32_t lastMs = 0;
    };
    Session sessions_[kMaxSessions];
    int failures_ = 0;
    uint32_t firstFailureMs_ = 0;
    uint32_t lockedUntilMs_ = 0;
    bool locked_ = false;
    hal::IRandom& rng_;
};

}  // namespace webapi
