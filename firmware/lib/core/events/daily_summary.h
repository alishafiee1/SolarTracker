#pragma once
// One log line per day instead of two lines per move (cycle 025): how many moves, how long the motor ran,
// the strongest wind, the lowest battery voltage and the biggest distance between the panel and its target.
// Pure code: main.cpp feeds it once a second and writes the line it returns.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace events {

class DailySummary {
public:
    // localDay = (utc + timezone offset) / 86400. moves / runMs are the running totals of the MotionController.
    // batV <= 0 means "no battery reading", errDeg < 0 means "no tracking error to report".
    // Returns true once when the day changes; `out` then holds the line for the day that just ended.
    bool onSecond(int64_t localDay, uint32_t moves, uint32_t runMs, float windKmh, float batV, float errDeg, char* out, size_t outSize) {
        bool wrote = false;
        if (day_ != localDay) {
            if (day_ >= 0) {
                char bat[16];
                if (minBat_ > 0.0f && minBat_ < 90.0f) snprintf(bat, sizeof(bat), "%.2f", static_cast<double>(minBat_));
                else snprintf(bat, sizeof(bat), "-");
                snprintf(out, outSize, "daily: moves=%u run_s=%u wind_max=%.0f bat_min=%s err_max=%.1f", static_cast<unsigned>(moves - startMoves_),
                         static_cast<unsigned>((runMs - startRunMs_) / 1000u), static_cast<double>(maxWind_), bat, static_cast<double>(maxErr_));
                wrote = true;
            }
            day_ = localDay;
            startMoves_ = moves;
            startRunMs_ = runMs;
            maxWind_ = 0.0f;
            minBat_ = 99.0f;
            maxErr_ = 0.0f;
        }
        if (windKmh > maxWind_) maxWind_ = windKmh;
        if (batV > 0.0f && batV < minBat_) minBat_ = batV;
        if (errDeg > maxErr_) maxErr_ = errDeg;
        return wrote;
    }

private:
    int64_t day_ = -1;
    uint32_t startMoves_ = 0;
    uint32_t startRunMs_ = 0;
    float maxWind_ = 0.0f;
    float minBat_ = 99.0f;
    float maxErr_ = 0.0f;
};

}  // namespace events
