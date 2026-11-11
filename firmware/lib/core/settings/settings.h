#pragma once
// Persistent settings (cycle 007, R1/R2). One versioned POD struct, stored as a blob with a CRC.
// Only append fields at the end and bump kSettingsVersion: older blobs are migrated by copying
// what they have and leaving the new fields at their defaults.

#include <hal.h>
#include <stddef.h>
#include <stdint.h>

namespace settings {

constexpr uint16_t kSettingsVersion = 1;

enum class Mode : uint8_t { Auto = 0, Manual = 1 };

struct Settings {
    // Location (R9). Default: Kerman.
    float latDeg = 30.283f;
    float lonDeg = 57.083f;
    char cityId[16] = "kerman";
    int16_t tzOffsetMin = 210;  // R5: +03:30, display only

    // Panel range and movement (005).
    float minDeg = -45.0f;
    float maxDeg = 45.0f;
    float moveThresholdDeg = 5.0f;
    uint32_t stallCheckMs = 5000;
    uint32_t preDawnMinutes = 0;

    // Motion timing (004).
    uint32_t stopSettleMs = 3000;
    uint32_t minMoveGapMs = 10000;
    uint32_t maxMoveMs = 120000;

    // Wind (006).
    float windStowKmh = 40.0f;
    uint32_t windStowDelayS = 5;
    float windReturnKmh = 30.0f;
    uint32_t windReturnMin = 15;
    float anemometerFactor = 6.8f;  // km/h per Hz of rotor rotation, calibrated on site

    Mode mode = Mode::Auto;
    float supplyLowV = 4.5f;  // R8

    // Access (008). The default AP password is provisioned outside the repository.
    char apPassword[33] = "";
    uint8_t settingsPasswordHash[32] = {};
    uint8_t settingsPasswordSalt[16] = {};
    bool passwordIsDefault = true;

    // Optional current module (cycle 009). Appended after the first layout: older blobs get these defaults.
    bool currentPresent = false;   // set when the current transformer is fitted
    float currentFactor = 5.0f;    // amps per volt (rms) at the ADC; SCT-013-005 gives 1 V at 5 A; calibrate with a clamp meter
    float stallCurrentA = 4.5f;
    float maxCurrentA = 5.0f;
    float zeroCurrentA = 0.2f;

    // Clock: true = the device follows the internet time (NTP) and the open panel pushes the computer/phone time.
    // False = the clock is only changed by hand. Appended after the first layout: older blobs get true.
    bool clockAuto = true;
};

// Names of invalid fields (R1). Returns how many were written (at most `max`).
size_t validate(const Settings& s, const char** invalid, size_t max);
inline bool isValid(const Settings& s) {
    const char* tmp[1];
    return validate(s, tmp, 1) == 0;
}

enum class LoadResult : uint8_t { Loaded, Reset, Migrated };

// Typed access on top of a blob store (NVS on the device).
class SettingsStore {
public:
    explicit SettingsStore(hal::ISettingsStore& store) : store_(store) {}

    // R2: missing or corrupt -> defaults + SettingsReset; older layout -> migrated + SettingsMigrated.
    LoadResult load(Settings& out, hal::IEventLog* log = nullptr);

    // R1: rejects invalid values and keeps the stored ones. `badField` (optional) gets the first invalid name.
    bool save(const Settings& s, const char** badField = nullptr);

private:
    hal::ISettingsStore& store_;
};

uint32_t crc32(const void* data, size_t len);

}  // namespace settings
