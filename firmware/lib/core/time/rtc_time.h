#pragma once
// Calendar maths for the DS3231 (cycle 007, R3/R4): UTC epoch <-> date and time, BCD.
#include <stdint.h>

namespace rtc {

struct DateTime {
    int year;  // 2000..2099 (the DS3231 keeps two digits plus a century bit)
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

inline uint8_t toBcd(int v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); }
inline int fromBcd(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }

DateTime fromEpoch(int64_t epochSec);
int64_t toEpoch(const DateTime& dt);
bool isPlausible(const DateTime& dt);  // field ranges and a year of 2024..2099

}  // namespace rtc
