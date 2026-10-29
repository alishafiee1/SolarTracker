#pragma once
// Host-side fakes of every HAL interface (cycle 001, R3). Tests drive them
// directly; the simulator (cycle 013) builds physical models on top.

#include <hal.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace sim {

class FakeTime : public hal::ITime {
public:
    uint32_t nowMs() const override { return nowMs_; }
    void set(uint32_t ms) { nowMs_ = ms; }
    void advance(uint32_t ms) { nowMs_ += ms; }

private:
    uint32_t nowMs_ = 0;
};

class FakeClock : public hal::IClock {
public:
    int64_t utc() const override { return utc_; }
    bool setUtc(int64_t epochSec) override {
        utc_ = epochSec;
        lostPower_ = false;
        return true;
    }
    bool lostPower() const override { return lostPower_; }
    void setLostPower(bool lost) { lostPower_ = lost; }

private:
    int64_t utc_ = 0;
    bool lostPower_ = false;
};

class FakeAccel : public hal::IAccel {
public:
    bool read(hal::Vec3& g) override {
        if (!ok_) return false;
        g = g_;
        return true;
    }
    bool ok() const override { return ok_; }
    void setGravity(hal::Vec3 g) { g_ = g; }
    void setOk(bool ok) { ok_ = ok; }

private:
    hal::Vec3 g_{0.0f, 0.0f, 1.0f};
    bool ok_ = true;
};

class FakeWindPulse : public hal::IWindPulse {
public:
    size_t drain(uint32_t* timestampsUs, size_t max) override {
        size_t n = 0;
        while (n < max && n < pending_.size()) {
            timestampsUs[n] = pending_[n];
            ++n;
        }
        pending_.erase(pending_.begin(), pending_.begin() + static_cast<long>(n));
        return n;
    }
    void pulse(uint32_t timestampUs) { pending_.push_back(timestampUs); }

private:
    std::vector<uint32_t> pending_;
};

// Records every relay write and counts interlock violations (direction change
// requested while powered). Starts with both relays inactive, like the board.
class FakeRelays : public hal::IRelays {
public:
    struct Write {
        uint32_t atMs;
        bool power;
        hal::Dir dir;
    };

    explicit FakeRelays(const hal::ITime* time = nullptr) : time_(time) {}

    void setPower(bool on) override {
        power_ = on;
        record();
    }
    bool setDirection(hal::Dir dir) override {
        if (power_) {
            ++violations_;
            return false;
        }
        dir_ = dir;
        record();
        return true;
    }
    bool power() const override { return power_; }
    hal::Dir direction() const override { return dir_; }

    const std::vector<Write>& writes() const { return writes_; }
    int violations() const { return violations_; }

private:
    void record() { writes_.push_back({time_ ? time_->nowMs() : 0, power_, dir_}); }

    const hal::ITime* time_;
    bool power_ = false;
    hal::Dir dir_ = hal::Dir::East;
    std::vector<Write> writes_;
    int violations_ = 0;
};

class FakeCurrentSensor : public hal::ICurrentSensor {
public:
    bool present() const override { return present_; }
    float rmsAmps() const override { return amps_; }
    void setPresent(bool present) { present_ = present; }
    void setAmps(float amps) { amps_ = amps; }

private:
    bool present_ = true;
    float amps_ = 0.0f;
};

class FakeSupplyMonitor : public hal::ISupplyMonitor {
public:
    float volts() const override { return volts_; }
    void setVolts(float v) { volts_ = v; }

private:
    float volts_ = 5.0f;
};

class FakeSettingsStore : public hal::ISettingsStore {
public:
    bool load(const char* key, void* buf, size_t len) override {
        auto it = blobs_.find(key);
        if (it == blobs_.end() || it->second.size() != len) return false;
        std::copy(it->second.begin(), it->second.end(), static_cast<uint8_t*>(buf));
        return true;
    }
    size_t size(const char* key) override {
        auto it = blobs_.find(key);
        return it == blobs_.end() ? 0 : it->second.size();
    }
    bool save(const char* key, const void* buf, size_t len) override {
        const auto* bytes = static_cast<const uint8_t*>(buf);
        blobs_[key] = std::vector<uint8_t>(bytes, bytes + len);
        return true;
    }

private:
    std::map<std::string, std::vector<uint8_t>> blobs_;
};

class FakeEventLog : public hal::IEventLog {
public:
    void append(hal::EventType type, const char* detail) override { events_.emplace_back(type, detail ? detail : ""); }
    const std::vector<std::pair<hal::EventType, std::string>>& events() const { return events_; }

private:
    std::vector<std::pair<hal::EventType, std::string>> events_;
};

}  // namespace sim
