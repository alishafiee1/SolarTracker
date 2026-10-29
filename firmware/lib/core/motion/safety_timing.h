#pragma once
#include <stdint.h>

namespace motion {

// Timing interlocks of the actuator (cycle 004). Defaults are the proposal's values;
// the settable ones come from Settings (cycle 007).
struct Timing {
    uint32_t relaySelectMs = 200;    // R2: wait between choosing the direction (CH2) and powering (CH1)
    uint32_t stopSettleMs = 3000;    // R3: wait after a stop (settable 1000..10000)
    uint32_t minMoveGapMs = 10000;   // R5: minimum time between the end of one move and the start of the next
    uint32_t maxMoveMs = 120000;     // R6: longest single move
    uint32_t reverseStableMs = 1000; // R4: the angle must have been steady this long before reversing
    float stableAngleDeg = 0.5f;     // R4: "steady" = changed less than this
    float zeroCurrentA = 0.2f;       // R4: "no current" threshold (only used with a current sensor)
    bool currentPresent = false;     // optional module 009
};

}  // namespace motion
