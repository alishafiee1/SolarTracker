#pragma once
// Hardware abstraction layer (cycle 001, R3). Core logic sees the hardware only
// through these interfaces; lib/drivers implements them for the ESP32 and
// lib/sim fakes them for host tests.

#include <stddef.h>
#include <stdint.h>

namespace hal {

// Monotonic milliseconds since boot (wraps after ~49 days).
class ITime {
public:
    virtual ~ITime() = default;
    virtual uint32_t nowMs() const = 0;
};

// Real-time clock. Always holds UTC.
class IClock {
public:
    virtual ~IClock() = default;
    virtual int64_t utc() const = 0;
    virtual bool setUtc(int64_t epochSec) = 0;
    // True when the oscillator stopped (dead battery, first start).
    virtual bool lostPower() const = 0;
};

struct Vec3 {
    float x;
    float y;
    float z;
};

// Accelerometer on the control board, values in g.
class IAccel {
public:
    virtual ~IAccel() = default;
    virtual bool read(Vec3& g) = 0;
    virtual bool ok() const = 0;
};

// Anemometer pulses: timestamps (micros) collected since the last call.
class IWindPulse {
public:
    virtual ~IWindPulse() = default;
    virtual size_t drain(uint32_t* timestampsUs, size_t max) = 0;
};

enum class Dir : uint8_t { East, West };

// Two relays: CH1 actuator power, CH2 direction (changeover contact).
class IRelays {
public:
    virtual ~IRelays() = default;
    virtual void setPower(bool on) = 0;
    // Refused (returns false) while power is on: CH2 never switches under load.
    virtual bool setDirection(Dir dir) = 0;
    virtual bool power() const = 0;
    virtual Dir direction() const = 0;
};

class ICurrentSensor {
public:
    virtual ~ICurrentSensor() = default;
    virtual bool present() const = 0;
    virtual float rmsAmps() const = 0;
};

class ISupplyMonitor {
public:
    virtual ~ISupplyMonitor() = default;
    virtual float volts() const = 0;
};

// Byte blobs by key (NVS on the device). Typed settings are built on top (cycle 007).
class ISettingsStore {
public:
    virtual ~ISettingsStore() = default;
    virtual bool load(const char* key, void* buf, size_t len) = 0;
    virtual bool save(const char* key, const void* buf, size_t len) = 0;
    // Stored length of a key, 0 if missing (lets callers read blobs written by an older layout).
    virtual size_t size(const char* key) = 0;
};

// Random bytes (hardware RNG on the device); used for session tokens and password salts.
class IRandom {
public:
    virtual ~IRandom() = default;
    virtual void bytes(uint8_t* out, size_t len) = 0;
};

enum class EventType : uint8_t {
    PowerUp,
    StateChange,
    Fault,
    FaultReset,
    MoveStart,
    MoveEnd,
    ClockSet,
    SettingsChange,
    SettingsReset,
    SettingsMigrated,
    SupplyLow,
    LogReset,
};

class IEventLog {
public:
    virtual ~IEventLog() = default;
    virtual void append(EventType type, const char* detail) = 0;
};

}  // namespace hal
