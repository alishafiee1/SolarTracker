#pragma once
#include <hal.h>

namespace drivers {

// Blobs in NVS namespace "suntracker" (cycle 007, R1). Keys are at most 15 characters.
class NvsSettingsStore : public hal::ISettingsStore {
public:
    bool load(const char* key, void* buf, size_t len) override;
    bool save(const char* key, const void* buf, size_t len) override;
    size_t size(const char* key) override;
};

}  // namespace drivers
