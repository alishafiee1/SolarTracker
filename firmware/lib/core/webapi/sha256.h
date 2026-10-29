#pragma once
// SHA-256 (FIPS 180-4), small and dependency free. Used for the salted settings-password hash.
#include <stddef.h>
#include <stdint.h>

namespace webapi {

struct Sha256 {
    Sha256();
    void update(const void* data, size_t len);
    void final(uint8_t out[32]);

private:
    void block(const uint8_t* p);
    uint32_t h_[8];
    uint8_t buf_[64];
    size_t bufLen_ = 0;
    uint64_t total_ = 0;
};

// sha256(salt || password)
void saltedHash(const uint8_t* salt, size_t saltLen, const char* password, uint8_t out[32]);

}  // namespace webapi
