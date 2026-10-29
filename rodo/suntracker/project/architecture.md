# معماری

> Technical language: English. Source: proposal v1.8 (`docs/solar_tracker_proposal.html`), sections 02, 04, 05, 06.
> Hardware (schematic, PCB, enclosure) lives in `hardware/` and `enclosure/` and is outside these software cycles.
> Status: planned. No firmware code exists yet; cycle 001 creates the layout below.

## بخش‌ها

| Part | Technology | Path | Why |
|---|---|---|---|
| Firmware | C++17, Arduino-ESP32 core on PlatformIO (`esp32dev`, ESP32-WROOM-32, 4 MB flash) | `firmware/` | Mature drivers for DS3231/MPU6050, async web server, OTA and NVS without ESP-IDF boilerplate |
| Core logic (pure, no Arduino headers) | Plain C++ | `firmware/lib/core/` | Sun position, angle math, state machine, motion safety, wind filter. Compiles on the host, so it is unit-tested and simulated |
| Hardware abstraction (HAL) | C++ interfaces | `firmware/lib/hal/` | `IClock`, `IAccel`, `IWindPulse`, `IRelays`, `ICurrentSensor`, `ISupplyMonitor`, `ISettingsStore`, `IEventLog`, `ITime` (monotonic ms) |
| Drivers (ESP32 only) | Arduino libraries | `firmware/lib/drivers/` | RTClib (DS3231), MPU6050 over raw I2C, GPIO relays, ADC for SCT-013, Preferences (NVS), LittleFS |
| Web panel | ESPAsyncWebServer + JSON REST; static HTML/CSS/JS (Persian, RTL), gzipped and embedded in the firmware image | `firmware/lib/web/`, `firmware/web/` | One `.bin` holds firmware and panel (needed for offline update) |
| Host tests and simulator | PlatformIO `native` env + Unity; fake HAL + actuator/wind/sensor model | `firmware/test/`, `firmware/lib/sim/` | Proposal phase D1: test movement logic before installation |

## Runtime structure

- One cooperative main loop with a fixed 100 ms control tick (`ControlLoop::tick(nowMs)`), plus the async web server task and the anemometer ISR (it only stores pulse timestamps).
- Control priority (highest wins): `Fault > Wind > Manual > Night > Auto`.
- `MotionController` is the only code that touches the relays. Everyone else asks it for `stop()` or `move(direction)`. It enforces every timing interlock.
- Boot order: relays OFF (inputs pulled up, active-low) → watchdog on → load settings → sensors → control loop → Wi-Fi AP → web server.

## Pin map (proposal section 04)

| GPIO | Use |
|---|---|
| 21 / 22 | I2C SDA / SCL — DS3231 @0x68, MPU6050 @0x69 (AD0 high) |
| 4 | Anemometer Hall pulse (interrupt, one pulse per rotor revolution; 20 cm cup rotor) |
| 25 | Relay CH1 — actuator power ON/OFF, active-low |
| 26 | Relay CH2 — direction east/west, active-low |
| 34 | ADC1_CH6 — 5 V supply divider |
| 32 | ADC1_CH4 — SCT-013 current, 1.65 V bias |

## دادهٔ اصلی

- `Settings` (NVS, versioned struct with defaults and range checks): location (lat, lon, city id; default Kerman), timezone offset (default +03:30), min/max angle (−45/+45°), move threshold (5°), wind stow/return thresholds (40/30 km/h), stow delay (5 s), return delay (15 min), anemometer factor, stall check time (5 s), max move time (120 s), stop settle time (3 s), minimum gap between moves (10 s), angle calibration (two gravity vectors), operating mode (auto/manual), AP password and settings password (same default on every device, both changeable; the settings password stored as a hash).
- `Event` (LittleFS rotating log): UTC timestamp, type (fault, move start/end, mode change, power restored, wind stow/return, settings change, update), short detail.
- `Status` (RAM, served at `/api/status`): current and target angle, wind speed, current RMS, mode, state, last fault.
- Time: the DS3231 always holds UTC; local time = UTC + stored offset.

## تست

- Unit and scenario tests run on the host: `pio test -d firmware -e native`. One cycle: `testCommand` in `project.json` (`pio test -d firmware -e native -f "test_cycle_{NNN}*"`).
- Every `acceptance.feature` scenario has a Unity test with the same name in `firmware/test/test_cycle_NNN_*/`, driven by the simulator (fake HAL + simulated time). The Gherkin files are the spec; there is no Gherkin runner and no `playwright-bdd`.
- Hardware-in-the-loop and field tests (proposal section 06: each scenario 3 times, 72 h soak, one-day tracking log) are run by hand and recorded in the cycle Progress notes (`- Verified:`).

## قواعدی که همه باید بدانند

- Safety first: relays are OFF after every boot, reset and watchdog trip; CH2 (direction) never switches under load; every timing interlock also applies in manual and wind modes.
- Angles are in degrees. Panel rotation `R` is positive towards **west**; 0° = horizontal (stow).
- Time is stored and computed in UTC; only the web panel shows local time.
- Core logic never includes `Arduino.h` and never calls `millis()` directly; time and sensors come through the HAL.
- No secrets in the repo or in rodo files: Wi-Fi and settings passwords and the Bale bot token live only in NVS on the device.
- Code map: Graphify (`graphify update .` → `graphify-out/`, not committed). Use `graphify affected "<symbol>"` for Documentation Impact.
