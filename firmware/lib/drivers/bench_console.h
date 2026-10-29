#pragma once
// Bench-only serial console (env esp32dev-bench, -DBENCH_CONSOLE). It lets a
// developer check the real board over USB. It is never part of the production
// image: the esp32dev env does not define BENCH_CONSOLE.
#include <stdint.h>

namespace drivers {

struct BenchStatus {
    const char* version;
    const char* resetReason;
    uint32_t uptimeMs;
    uint32_t ticks;
    uint32_t lastTickMs;
};

// Call once per loop pass. Commands (one per line):
//   status   one-line state: version, reset reason, uptime, tick count, relay pin levels
//   pins     relay pin levels (1 = inactive/high, 0 = active/low)
//   hang     stop feeding the watchdog; the chip must reset with reason "watchdog"
//   reboot   software reset (reason "software")
//   help
void benchConsolePoll(const BenchStatus& status);

}  // namespace drivers
