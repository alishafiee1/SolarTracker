#include "sun_position.h"

#include <math.h>

namespace sun {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kRad = kPi / 180.0;
constexpr double kEarthMeanRadiusKm = 6371.01;
constexpr double kAstronomicalUnitKm = 149597890.0;
// 2000-01-01 12:00:00 UTC (JD 2451545.0) as a Unix timestamp.
constexpr double kJ2000Epoch = 946728000.0;

}  // namespace

SunPosition position(int64_t utcEpochSec, double latDeg, double lonDeg) {
    const double secOfDay = static_cast<double>(((utcEpochSec % 86400) + 86400) % 86400);
    const double decimalHours = secOfDay / 3600.0;
    const double n = (static_cast<double>(utcEpochSec) - kJ2000Epoch) / 86400.0;  // days since J2000.0

    // Ecliptic coordinates.
    const double omega = 2.1429 - 0.0010394594 * n;
    const double meanLongitude = 4.8950630 + 0.017202791698 * n;
    const double meanAnomaly = 6.2400600 + 0.0172019699 * n;
    const double eclipticLongitude = meanLongitude + 0.03341607 * sin(meanAnomaly) +
                                     0.00034894 * sin(2.0 * meanAnomaly) - 0.0001134 - 0.0000203 * sin(omega);
    const double obliquity = 0.4090928 - 6.2140e-9 * n + 0.0000396 * cos(omega);

    // Celestial coordinates.
    const double sinEclLon = sin(eclipticLongitude);
    double rightAscension = atan2(cos(obliquity) * sinEclLon, cos(eclipticLongitude));
    if (rightAscension < 0.0) rightAscension += kTwoPi;
    const double declination = asin(sin(obliquity) * sinEclLon);

    // Local coordinates.
    const double gmstHours = 6.6974243242 + 0.0657098283 * n + decimalHours;
    const double lmst = (gmstHours * 15.0 + lonDeg) * kRad;
    const double hourAngle = lmst - rightAscension;
    const double lat = latDeg * kRad;
    const double cosLat = cos(lat);
    const double sinLat = sin(lat);
    const double cosHour = cos(hourAngle);

    double zenith = acos(cosLat * cosHour * cos(declination) + sin(declination) * sinLat);
    double azimuth = atan2(-sin(hourAngle), tan(declination) * cosLat - sinLat * cosHour);
    if (azimuth < 0.0) azimuth += kTwoPi;

    // Parallax correction.
    zenith += (kEarthMeanRadiusKm / kAstronomicalUnitKm) * sin(zenith);

    return {azimuth / kRad, zenith / kRad};
}

}  // namespace sun
