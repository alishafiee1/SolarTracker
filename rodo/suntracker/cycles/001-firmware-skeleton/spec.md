# 001 — Firmware skeleton and HAL: technical spec

> Source of truth for the coder. Anything not here is not expected. Read together with `design.md`.

## 1. Summary

Create the PlatformIO firmware project, the layered layout (core / hal / drivers / sim / web), a safe boot sequence, the watchdog and a 100 ms control tick. No tracking logic yet.

## 2. Rules

- **R1** `firmware/platformio.ini` has two environments: `esp32dev` (platform espressif32, framework arduino, board esp32dev, C++17) and `native` (host, Unity test framework). `pio run -d firmware -e esp32dev` and `pio test -d firmware -e native` both succeed on a clean checkout.
- **R2** Layout: `src/main.cpp` (wiring only), `lib/core/` (pure logic), `lib/hal/` (interfaces), `lib/drivers/` (ESP32 implementations), `lib/sim/` (fakes and simulator), `lib/web/` (empty placeholder), `test/`. `lib/core` and `lib/hal` must compile in `native` and must not include `Arduino.h`; `lib/drivers` and `lib/web` are excluded from `native` (`lib_ignore` / `build_src_filter`).
- **R3** HAL interfaces (pure virtual, in `lib/hal/`): `ITime` (monotonic `uint32_t nowMs()`), `IClock` (UTC epoch get/set, `lostPower()`), `IAccel` (raw x/y/z in g, `ok()`), `IWindPulse` (pulse count since last call), `IRelays` (`setPower(bool)`, `setDirection(Dir)`, read-back of last commanded state), `ICurrentSensor` (`rmsAmps()`, `present()`), `ISupplyMonitor` (`volts()`), `ISettingsStore`, `IEventLog` (`append(type, detail)`). Each has a fake in `lib/sim/` that tests can drive.
- **R4** Safe boot: the first lines of `setup()` write GPIO 25 and 26 HIGH (relay inactive, active-low) and only then set them to OUTPUT, so the pins never drive LOW during boot. No other code runs before this.
- **R5** Task watchdog enabled with a 5 s timeout on the loop task; the loop feeds it once per pass. On boot the reset reason (`esp_reset_reason()`) is read and printed on serial, and kept in RAM for the event log (cycle 007).
- **R6** `ControlLoop::tick(uint32_t nowMs)` is called every 100 ms (±10 ms) from `loop()` using `ITime`; it is empty in this cycle. Nothing in `loop()` may block for more than 50 ms.
- **R7** Firmware version comes from a build flag `FW_VERSION` (from `git describe --tags --always`, via a PlatformIO extra script) and is printed at boot as `SunTracker <version> — reset: <reason>`.
- **R8** Custom `partitions.csv` for 4 MB flash: `nvs` (20 KB), `otadata` (8 KB), `ota_0` and `ota_1` (≥ 1.5 MB each), `littlefs` (≥ 512 KB), `coredump` (64 KB). Chosen now so cycle 010 needs no repartition.
- **R9** Serial at 115200 baud; log lines are prefixed with a level (`I`, `W`, `E`).

## 3. Data

None persisted in this cycle.

## 4. API / logic

- `src/main.cpp`: `setup()` = R4 → R5 → create drivers → create `ControlLoop`; `loop()` = feed watchdog → tick scheduler.
- `lib/core/control_loop.h/.cpp`: `class ControlLoop { void tick(uint32_t nowMs); }`.

## 5. Screens

None. Serial output only (R7).

## 6. Files to create or change

- `firmware/platformio.ini`, `firmware/partitions.csv`, `firmware/scripts/version.py`
- `firmware/src/main.cpp`
- `firmware/lib/core/control_loop.{h,cpp}`
- `firmware/lib/hal/*.h` (interfaces from R3)
- `firmware/lib/sim/fake_*.h` (fakes from R3)
- `firmware/lib/drivers/relays_gpio.{h,cpp}` (GPIO 25/26, active-low)
- `firmware/test/test_cycle_001_skeleton/test_main.cpp`
- `README.md` — build and test commands

## 7. Edge cases

- Brown-out reset during relay switching: relays must come up OFF (covered by R4 and the hardware pull-ups).
- Watchdog reset loop: if the device resets by watchdog 3 times within 10 minutes, it stays in a safe idle (relays off, no tracking) and reports it (stored in RTC memory; full handling in 005).

## 8. Acceptance run

`rodo test 001` → `pio test -d firmware -e native -f "test_cycle_001*"`. Boot timing (R4) is checked once on hardware with a scope or LED on the relay inputs.

## Acceptance criteria

- **Must happen:** both environments build (R1); relays are inactive from power-on (R4); a forced hang triggers a watchdog reset and the reset reason is printed (R5); the tick runs every 100 ms (R6); version printed (R7).
- **Must not happen:** `Arduino.h` included from `lib/core` or `lib/hal`; any relay pulse during boot.
- **Done when:** `test_cycle_001*` passes on native, `pio run -e esp32dev` succeeds, and the boot check on a dev board is recorded in Progress notes.

## Documentation Impact

- `README.md` — build/test commands and the real `firmware/` layout replace "(planned)".
- `rodo/suntracker/project/architecture.md` — confirm paths and library names once they exist.

## Open Questions

- none

## Decisions

- Arduino-ESP32 on PlatformIO, not ESP-IDF. Because the drivers and web server we need exist and the proposal budget (27 h base firmware) leaves no room for IDF boilerplate. (default, not asked)
- Cooperative 100 ms loop instead of several FreeRTOS tasks for control. Because one thread of control makes the relay interlocks easy to prove; only the web server and the anemometer ISR run apart. (default, not asked)
- Partition table with two OTA slots from day one. Because adding OTA later would need a USB reflash of every device.
