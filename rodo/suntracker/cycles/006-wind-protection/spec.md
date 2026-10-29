# 006 — High-wind protection: technical spec

## 1. Summary

Count anemometer pulses, convert to km/h, and raise/clear a `windStow` flag with time-based hysteresis. The state machine (005) acts on the flag.

## 2. Rules

- **R1** ISR on GPIO 4 (falling edge) increments a volatile counter; pulses closer than 2 ms to the previous one are ignored (debounce). The ISR does nothing else.
- **R2** Every 1 s: frequency = pulses / s; speed km/h = frequency × `anemometerFactor` (setting, default 2.4 km/h per Hz, to be confirmed with the chosen anemometer). The control logic uses the average of the last 3 s.
- **R3** `windStow` becomes true when speed > `windStowKmh` (default 40) continuously for > `windStowDelayS` (default 5 s).
- **R4** `windStow` becomes false only when speed < `windReturnKmh` (default 30) continuously for `windReturnMin` (default 15 min). Any sample ≥ `windReturnKmh` restarts that timer.
- **R5** `windStow` is independent of the operating mode (auto/manual); 005 gives it priority over manual.
- **R6** Validation: `windReturnKmh` < `windStowKmh`; 10 ≤ stow ≤ 100 km/h; factor 0.1–10.
- **R7** Wind stow start and end produce events with the peak speed.

## 3. Data

Settings: `anemometerFactor`, `windStowKmh`, `windStowDelayS`, `windReturnKmh`, `windReturnMin`.

## 4. API / logic

`class WindMonitor { void onSecond(uint32_t pulses); float speedKmh() const; bool stow() const; }` (pure core) and `Anemometer : IWindPulse` driver.

## 5. Screens

Status in 008: «باد: n km/h — عادی» / «باد شدید — حالت ایمن (بازگشت در mm:ss)».

## 6. Files to create or change

- `firmware/lib/core/wind/wind_monitor.{h,cpp}`
- `firmware/lib/drivers/anemometer.{h,cpp}`
- `firmware/lib/sim/wind_profile.h` (replay a speed-vs-time profile)
- `firmware/test/test_cycle_006_wind/test_main.cpp`

## 7. Edge cases

- Noise spikes on a long cable → debounce (R1) + 3 s average.
- Wind exactly at 40 km/h → not stow (strictly greater).
- Reboot during wind stow → starts with `windStow` false but speed is re-measured within 5 s; acceptable.

## Acceptance criteria

- **Must happen:** proposal scenario 2 in the simulator (stow after > 5 s above 40; return after 15 min below 30).
- **Must not happen:** stow on a 4 s gust; return while gusts ≥ 30 continue.
- **Done when:** `test_cycle_006*` passes; on hardware, a spun anemometer shows a plausible speed (recorded).

## Documentation Impact

- none

## Open Questions

- Which anemometer is bought (local build or ready pulse anemometer)? — default if no answer: factor 2.4 km/h per Hz, calibrated on site against a handheld anemometer.

## Decisions

- Decisions on raw 3 s average, not gust peaks. Because the proposal thresholds are sustained-wind thresholds with explicit delays. (default, not asked)
