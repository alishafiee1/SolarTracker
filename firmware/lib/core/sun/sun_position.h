#pragma once
#include <stdint.h>

namespace sun {

struct SunPosition {
    double azimuthDeg;  // from north, clockwise, [0, 360)
    double zenithDeg;   // 0 = overhead, 90 = horizon (geometric, no refraction)
};

// Sun position for a UTC time and a site (cycle 002, R1).
// Longitude is east-positive. Algorithm: PSA (Blanco-Muriel et al., 2001).
SunPosition position(int64_t utcEpochSec, double latDeg, double lonDeg);

// True when the sun is below the horizon (R5).
inline bool isNight(const SunPosition& p) { return p.zenithDeg > 90.0; }

}  // namespace sun
