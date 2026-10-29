# 006 — High-wind protection: technical spec

## 1. Summary

Count anemometer pulses, convert to km/h, and raise/clear a `windStow` flag with time-based hysteresis. The state machine (005) acts on the flag.

## 2. Rules

- **R1** Sensor: one Hall sensor; the rotor magnet passes it once per revolution (1 pulse per revolution). ISR on GPIO 4 (falling edge) stores the `micros()` timestamp of each pulse in a small ring buffer (16 entries); pulses closer than 5 ms to the previous one are ignored (debounce; 5 ms = 200 rev/s, far above any real wind). The ISR does nothing else.
- **R2** With only ~6 pulses/s at 40 km/h, counting pulses per second is too coarse, so speed is computed from pulse periods: every 1 s, rotation frequency f = (pulses in the last 3 s − 1) / (time between the first and last of them); no pulse for 3 s → 0 km/h. Speed km/h = f × `anemometerFactor`. Default factor 6.8 km/h per Hz: rotor diameter 20 cm → cup-centre circumference 0.628 m, × cup factor ≈ 3 → 1.88 m/s per Hz. The factor is calibrated on site against a handheld anemometer (R6 range).
- **R3** `windStow` becomes true when speed > `windStowKmh` (default 40) continuously for > `windStowDelayS` (default 5 s).
- **R4** `windStow` becomes false only when speed < `windReturnKmh` (default 30) continuously for `windReturnMin` (default 15 min). Any sample ≥ `windReturnKmh` restarts that timer.
- **R5** `windStow` is independent of the operating mode (auto/manual); 005 gives it priority over manual.
- **R6** Validation: `windReturnKmh` < `windStowKmh`; 10 ≤ stow ≤ 100 km/h; factor 1–20 km/h per Hz.
- **R7** Wind stow start and end produce events with the peak speed.

## 3. Data

Settings: `anemometerFactor`, `windStowKmh`, `windStowDelayS`, `windReturnKmh`, `windReturnMin`.

## 4. API / logic

`class WindMonitor { void onSecond(const PulseWindow& pulses); float speedKmh() const; bool stow() const; }` (pure core) and `Anemometer : IWindPulse` driver.

## 5. Screens

Status in 008: «باد: n km/h — عادی» / «باد شدید — حالت ایمن (بازگشت در mm:ss)».

## 6. Files to create or change

- `firmware/lib/core/wind/wind_monitor.{h,cpp}`
- `firmware/lib/drivers/anemometer.{h,cpp}`
- `firmware/lib/sim/wind_profile.h` (replay a speed-vs-time profile)
- `firmware/test/test_cycle_006_wind/test_main.cpp`

## 7. Edge cases

- Noise spikes on a long cable → debounce (R1) + 3 s window.
- Very light wind: rotor may stop between pulses → reported as 0 km/h after 3 s; harmless (far below thresholds).
- Wind exactly at 40 km/h → not stow (strictly greater).
- Reboot during wind stow → starts with `windStow` false but speed is re-measured within 5 s; acceptable.

## Acceptance criteria

- **Must happen:** proposal scenario 2 in the simulator (stow after > 5 s above 40; return after 15 min below 30).
- **Must not happen:** stow on a 4 s gust; return while gusts ≥ 30 continue.
- **Done when:** `test_cycle_006*` passes; on hardware, a spun anemometer shows a plausible speed (recorded).

## Documentation Impact

- none

## Open Questions

- none

## Decisions

- Debounce (5 ms) runs twice: in the interrupt (cheap, keeps the ring buffer small) and in `WindMonitor::feed` (testable on the host). Because the same rule must hold in both places and only one of them can be unit-tested.
- The stow flag is not stored: after a reboot the panel starts calm and the wind is measured again within seconds (the 5 s delay applies again). Because a stale flag after a long power cut would keep the panel flat for no reason, and a re-measured strong wind stows it again at once.
- Stow start and end are logged as `StateChange` events with the text `wind stow start/end, peak N km/h` instead of a new event type. Because the log format is fixed and the state machine already logs `Auto->Wind`.
- On the board the pulse input is tested by letting the bench console pull the real pin low (`windgen`), which runs the real interrupt, ring buffer and speed code. A real anemometer still has to be calibrated on site against a handheld meter.
- Anemometer: locally built cup rotor, 20 cm diameter, one Hall pulse per revolution; default factor 6.8 km/h per Hz, calibrated on site. Because that is the hardware the user chose (answer of 2026-10-29).
- Speed from pulse periods, not pulse counts. Because one pulse per revolution gives only ~6 pulses/s at the 40 km/h threshold.
- Decisions on raw 3 s average, not gust peaks. Because the proposal thresholds are sustained-wind thresholds with explicit delays. (default, not asked)
