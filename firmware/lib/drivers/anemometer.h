#pragma once
#include <hal.h>

namespace drivers {

// Hall sensor on GPIO 4, one falling edge per rotor revolution (cycle 006, R1).
// The interrupt only stores a micros() timestamp; debounce and speed are computed in the main loop.
class Anemometer : public hal::IWindPulse {
public:
    static constexpr int kPin = 4;

    void begin();
    size_t drain(uint32_t* timestampsUs, size_t max) override;
    // Pulses seen since boot (for the bench console).
    uint32_t total() const;
};

}  // namespace drivers
