#pragma once
// Bench-only serial console (env esp32dev-bench, -DBENCH_CONSOLE). It lets a
// developer check the real board over USB. It is never part of the production
// image: the esp32dev env does not define BENCH_CONSOLE.
#include <stdint.h>

#include "angle_provider.h"
#include "control_state_machine.h"
#include "ds3231_clock.h"
#include "littlefs_event_log.h"
#include "motion_controller.h"
#include "mpu6050_accel.h"
#include "settings.h"

namespace drivers {

struct BenchStatus {
    const char* version;
    const char* resetReason;
    uint32_t uptimeMs;
    uint32_t ticks;
    uint32_t lastTickMs;
    Mpu6050Accel* accel;
    angle::AngleProvider* angle;
    motion::MotionController* motion;
    settings::Settings* cfg;
    settings::SettingsStore* store;
    Ds3231Clock* clock;
    control::ControlStateMachine* ctl;
    LittleFsEventLog* log;
};

// True after `move`/`stop`: the console then drives the MotionController directly and the state
// machine is not ticked (relay bench tests). Any `ctl ...` command gives control back.
bool benchDirectMotion();

// Event log that prints every event on the serial port (bench image only).
hal::IEventLog* benchEventLog();

// Call once per loop pass. Commands (one per line):
//   status    one-line state: version, reset reason, uptime, tick count, relay pin levels
//   sun [t]   sun position for Kerman at UTC epoch t (default 2027-06-21 07:00) and the time per call
//   angle     accelerometer state: WHO_AM_I, raw g, calibration, filtered angle, fault
//   cal h     start the horizontal calibration capture (panel horizontal, keep still for 5 s)
//   cal e     start the east calibration capture (panel turned a few degrees east)
//   cal       calibration state and last capture result
//   move e|w  ask the MotionController to move east / west (all interlocks apply)
//   stop      ask the MotionController to stop
//   motion    controller state, safety wait, fault and relay pin levels
//   settings  a few stored settings
//   set thr V  change the move threshold (degrees) and save it; `set reset` restores the defaults
//   clock     DS3231 state; `clock set <utc epoch>` sets it
//   log [n]   the newest n (default 10) event lines; `loggen n` writes n test events (rotation test)
//   ctl       state machine: state, fault, target, angle, status text
//   ctl auto|manual|e|w|stop|ka|reset   mode and manual commands (e/w need manual mode and a keep-alive every 3 s)
//   pins      relay pin levels (1 = inactive/high, 0 = active/low)
//   hang      stop feeding the watchdog; the chip must reset with reason "watchdog"
//   reboot    software reset (reason "software")
//   help
void benchConsolePoll(const BenchStatus& status);

}  // namespace drivers
