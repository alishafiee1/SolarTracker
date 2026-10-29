#pragma once
#include "sun_position.h"

namespace tracking {

constexpr double kDefaultMinDeg = -45.0;
constexpr double kDefaultMaxDeg = 45.0;

// Panel rotation R for a horizontal north-south axis, positive towards west
// (cycle 002, R3): R = atan2(sin z * sin(azimuth - 180), cos z), in degrees.
double rotation(const sun::SunPosition& p);

// Clamp R to the allowed range (R4). When the sun is outside the range the
// panel waits at the limit on the sun's side.
double target(double rotationDeg, double minDeg = kDefaultMinDeg, double maxDeg = kDefaultMaxDeg);

}  // namespace tracking
