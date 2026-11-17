# USB serial: what it prints and what it accepts

Settings: **115200 baud, 8N1**, one command per line (max 95 characters). Tool: `python firmware/tools/bench.py COM8 <command> ... [--json]`.
Opening the port with this tool does **not** reset the board (DTR and RTS are set before it opens). Since cycle 033 the console is in **every image**; the separate bench image only adds the `BENCH_CONSOLE` flag, which no longer changes anything.

Answers are one line each. Text answers start with `I ` (info) or `E ` (error). `json` and `selftest` answer with one JSON object (one line, starts with `{`), which is what an automation script should parse.

## Read commands (safe)

| Command | Answer |
|---|---|
| `json` | whole state in one JSON line: version, reset reason, uptime, heap, mode/state/fault, angle (degrees, ok, calibrated, warning, **faultCause** `i2c` or `gravity`, raw gravity, WHO_AM_I), power (battery V and %, mains V), wind, relay levels, clock, Wi-Fi |
| `selftest` | JSON: `i2c`, `gravity` (and `gravityG`), `rtc`, `relays_off`, `hotspot`, `battery_pin` (information only) and `pass` |
| `status`, `pins`, `motion`, `ctl` | the older one-line text states (version, relay pins, motion controller, state machine) |
| `angle` | WHO_AM_I, raw g, calibration, filtered angle, fault and its cause |
| `log [n]` | the newest n events from the flash log |
| `wind`, `current`, `clock`, `settings`, `sun [epoch]`, `ota` | wind speed and stow state, current sensor, DS3231, a few settings, sun position, probation state |
| `help` | list of commands |

## Control commands

| Command | Does |
|---|---|
| `ctl auto`, `ctl manual`, `ctl e`, `ctl w`, `ctl stop`, `ctl ka`, `ctl reset` | mode, manual moves, fault reset **through the state machine** (all interlocks apply) |
| `move e`, `move w`, `stop` | drive the motion controller directly (its interlocks and delays still apply) |
| `cal h`, `cal e`, `cal` | angle calibration steps and state |
| `set thr <deg>`, `set reset`, `clock set <utc>` | settings and clock |
| `reboot`, `hang` | software reset; stop feeding the watchdog (must reset with reason `watchdog`) |
| `loggen n`, `windgen <hz>`, `current on` | bench helpers: write test events, make the anemometer pin pulse, fit the current module |

## Bench (raw) mode: relays under the host's control, no interlocks

For testing with the **actuator disconnected**. `raw on` hands the two relay pins to the host, stops the control loop and skips every
interlock and delay (CH2 may change while CH1 is on, no settle time, no gap). The dead-man timer switches it off by itself.

| Command | Does |
|---|---|
| `raw on [seconds]` | enter raw mode for 1 to 600 s (default 60). Both relays are released first. The event log gets a line |
| `relay 1 0|1` | CH1 (power) off/on |
| `relay 2 0|1` | CH2 (direction) east/west |
| `relay all 0|1` | both |
| `raw off` | leave raw mode: both relays released, control loop resumes |

Every `relay` command extends the dead-man timer; when it runs out, or after a reboot, the relays are released and the log says `usb raw mode off (timeout)`.
Safety kept in raw mode: the watchdog, the boot sequence (relays off first) and the hardware wiring that never feeds both directions at once.
`move`, `ctl` are refused while raw mode is on.

## What the port prints without being asked

`I <version> reset: <reason>` at boot, `I hotspot <name> up, panel at http://192.168.4.1/`, and `E ...` lines for start-up problems.
Events (moves, faults, settings changes, updates, the reasons of sensor faults) are in the flash log: panel → ابزار → لاگ, `GET /api/log.csv` or `log n` here.

## Automation recipe

```
python firmware/tools/bench.py COM8 selftest --json          # after an update: pass must be true
python firmware/tools/bench.py COM8 json --json              # whole state
python firmware/tools/bench.py COM8 "raw on 30" "relay 1 1" "relay 1 0" "raw off"
```

## Where the rest lives

- `firmware/lib/drivers/bench_console.{h,cpp}`: the commands (the header comment is the short list; keep it in step with this file).
- `firmware/tools/bench.py`: send/read tool. `firmware/tools/bench_check_NNN.py`: one acceptance script per cycle (kept as the record of what was checked; new checks should use `bench.py`).
- `docs/acceptance-checklist.md`: where the console is used in the delivery checks.
