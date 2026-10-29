#include "tracking.h"

#include <math.h>

namespace tracking {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kRad = kPi / 180.0;
}  // namespace

double rotation(const sun::SunPosition& p) {
    const double z = p.zenithDeg * kRad;
    const double a = (p.azimuthDeg - 180.0) * kRad;
    return atan2(sin(z) * sin(a), cos(z)) / kRad;
}

double target(double rotationDeg, double minDeg, double maxDeg) {
    if (rotationDeg < minDeg) return minDeg;
    if (rotationDeg > maxDeg) return maxDeg;
    return rotationDeg;
}

}  // namespace tracking
