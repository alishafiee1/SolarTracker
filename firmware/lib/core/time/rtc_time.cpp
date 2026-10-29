#include "rtc_time.h"

namespace rtc {

namespace {
// Days from 1970-01-01 to a civil date (Howard Hinnant's algorithm).
int64_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
}  // namespace

int64_t toEpoch(const DateTime& dt) {
    return daysFromCivil(dt.year, dt.month, dt.day) * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
}

DateTime fromEpoch(int64_t t) {
    int64_t days = t / 86400;
    int64_t rem = t % 86400;
    if (rem < 0) {
        rem += 86400;
        --days;
    }
    days += 719468;
    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const int64_t doe = days - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t y = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    const int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    DateTime dt;
    dt.year = static_cast<int>(y + (m <= 2));
    dt.month = m;
    dt.day = d;
    dt.hour = static_cast<int>(rem / 3600);
    dt.minute = static_cast<int>((rem % 3600) / 60);
    dt.second = static_cast<int>(rem % 60);
    return dt;
}

bool isPlausible(const DateTime& dt) {
    if (dt.year < 2024 || dt.year > 2099) return false;
    if (dt.month < 1 || dt.month > 12 || dt.day < 1 || dt.day > 31) return false;
    if (dt.hour < 0 || dt.hour > 23 || dt.minute < 0 || dt.minute > 59 || dt.second < 0 || dt.second > 59) return false;
    return fromEpoch(toEpoch(dt)).day == dt.day;  // rejects 31 February and similar
}

}  // namespace rtc
