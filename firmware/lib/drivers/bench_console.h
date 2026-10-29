#pragma once
// Bench-only serial console (env esp32dev-bench, -DBENCH_CONSOLE). It lets a
// developer check the real board over USB. It is never part of the production
// image: the esp32dev env does not define BENCH_CONSOLE.
#include <stdint.h>

#include "angle_provider.h"
#include "mpu6050_accel.h"

namespace drivers {

struct BenchStatus {
    const char* version;
    const char* resetReason;
    uint32_t uptimeMs;
    uint32_t ticks;
    uint32_t lastTickMs;
    Mpu6050Accel* accel;
    angle::AngleProvider* angle;
};

// Call once per loop pass. Commands (one per line):
//   status    one-line state: version, reset reason, uptime, tick count, relay pin levels
//   sun [t]   sun position for Kerman at UTC epoch t (default 2027-06-21 07:00) and the time per call
//   angle     accelerometer state: WHO_AM_I, raw g, calibration, filtered angle, fault
//   cal h     start the horizontal calibration capture (panel horizontal, keep still for 5 s)
//   cal e     start the east calibration capture (panel turned a few degrees east)
//   cal       calibration state and last capture result
//   pins      relay pin levels (1 = inactive/high, 0 = active/low)
//   hang      stop feeding the watchdog; the chip must reset with reason "watchdog"
//   reboot    software reset (reason "software")
//   help
void benchConsolePoll(const BenchStatus& status);

}  // namespace drivers
