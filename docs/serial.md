# USB serial: what it prints and what it accepts

Two images, two behaviours. Settings: **115200 baud, 8N1**, one command per line (max 31 characters). One tool for both: `python firmware/tools/bench.py COM8 ...` (see its header; run it with `pio pkg exec -- python` so pyserial is found).

## Normal image (`esp32dev`, the one that is delivered)

Output only; it accepts no commands. Every line starts with a letter and a space: `I` info, `E` error.

| Line | When |
|---|---|
| `I <version> reset=<reason>` | boot banner (reason: power, software, watchdog, brownout, …) |
| `I hotspot <name> up, panel at http://192.168.4.1/` | hotspot started |
| `E hotspot failed to start…`, `E event log: flash not usable…` | start-up problems (the controller keeps working) |

Everything else (moves, faults, settings changes, updates) is in the event log: panel → ابزار → لاگ, or `GET /api/log.csv`.

## Bench image (`esp32dev-bench`, never delivered)

Build and flash: `pio run -d firmware -e esp32dev-bench -t upload`. It also prints every event live as `I event: type=<n> <text>`.
It drives real pins, so run it with the actuators disconnected unless a command says otherwise.

| Command | Does |
|---|---|
| `status` | version, reset reason, uptime, tick count, relay pin levels |
| `pins` | levels of CH1 (power, GPIO25) and CH2 (direction, GPIO26); 1 = inactive |
| `angle` | accelerometer: WHO_AM_I, raw g, calibration, filtered angle, fault |
| `cal` / `cal h` / `cal e` | calibration state / start the horizontal / east capture |
| `ctl` | state machine: state, fault, target, angle, status text |
| `ctl auto`, `ctl manual`, `ctl e`, `ctl w`, `ctl stop`, `ctl ka`, `ctl reset` | mode, manual moves (need manual mode and `ka` every 3 s), fault reset |
| `move e` / `move w` / `stop` | drive the MotionController directly (interlocks apply); any `ctl …` gives control back |
| `motion` | controller state, safety wait, fault, relay pins |
| `sun [epoch]` | sun position for Kerman and the time per call |
| `settings`, `set thr <deg>`, `set reset` | show / change the move threshold / restore defaults |
| `clock`, `clock set <utc epoch>` | DS3231 state / set it |
| `wind`, `windgen <hz>` | wind speed and stow state / make the real pin generate pulses (0 stops) |
| `current`, `current on` | SCT-013 reading / fit the module for this boot |
| `log [n]`, `loggen <n>`, `csv` | newest n events / write n test events / dump the log |
| `ota`, `ota arm` | probation state / put the running image on probation |
| `hang` | stop feeding the watchdog (must reset with reason `watchdog`) |
| `reboot` | software reset (reason `software`) |
| `help` | list of commands |

Answers are one `I <command>: key=value …` line each. Not covered yet: the six-face offsets (cycle 018), the angle warning (019) and the restart button (021); use the panel or `GET /api/offsets`, `GET /api/status` for those.

## Where the rest lives

- `firmware/lib/drivers/bench_console.{h,cpp}`: the commands (the header comment is the short list; keep it in step with this file).
- `firmware/tools/bench.py`: generic send/read tool. `firmware/tools/bench_check_NNN.py`: one acceptance script per cycle (each repeats the same serial helpers; they are kept as the record of what was checked, new checks should use `bench.py` instead).
- `docs/acceptance-checklist.md`: where the bench image is used in the delivery checks.
