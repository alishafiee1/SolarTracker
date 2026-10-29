#include "anemometer.h"

#include <Arduino.h>

namespace drivers {

namespace {
constexpr size_t kRing = 32;  // at 200 km/h the rotor makes about 30 pulses per second; drained every 100 ms
volatile uint32_t ring[kRing];
volatile uint32_t head = 0;  // written by the ISR
uint32_t tail = 0;           // read by drain()
volatile uint32_t totalPulses = 0;
volatile uint32_t lastUs = 0;
constexpr uint32_t kIsrDebounceUs = 5000;

void IRAM_ATTR onPulse() {
    const uint32_t now = micros();
    if (now - lastUs < kIsrDebounceUs) return;  // contact bounce
    lastUs = now;
    ring[head % kRing] = now;
    head = head + 1;
    totalPulses = totalPulses + 1;
}
}  // namespace

void Anemometer::begin() {
    pinMode(kPin, INPUT_PULLUP);  // the board also has an external pull-up, RC filter and TVS (proposal)
    attachInterrupt(digitalPinToInterrupt(kPin), onPulse, FALLING);
}

size_t Anemometer::drain(uint32_t* timestampsUs, size_t max) {
    size_t n = 0;
    const uint32_t h = head;
    if (h - tail > kRing) tail = h - kRing;  // the loop was too slow: drop the oldest
    while (tail != h && n < max) timestampsUs[n++] = ring[tail++ % kRing];
    return n;
}

uint32_t Anemometer::total() const { return totalPulses; }

}  // namespace drivers
