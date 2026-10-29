# 002 — Sun position and target angle: technical spec

## 1. Summary

Pure functions that turn UTC time and site coordinates into sun azimuth/zenith (PSA+) and then into the single-axis panel rotation, clamped to the allowed range.

## 2. Rules

- **R1** `SunPosition sun::position(int64_t utcEpochSec, double latDeg, double lonDeg)` implements PSA+ (Blanco et al., 2020). Output: azimuth γ in degrees from north, clockwise, in [0, 360); zenith z in degrees. Longitude east positive.
- **R2** Accuracy: compared with NREL SPA reference values for Kerman (30.283 N, 57.083 E) at ≥ 40 timestamps spread over 2026–2035 (all seasons, sunrise to sunset), the absolute error is ≤ 0.05° for both γ and z. Reference values are generated once with `pvlib` (SPA) by `firmware/tools/make_spa_fixture.py` and committed as `firmware/test/fixtures/spa_kerman.csv`.
- **R3** `double tracking::rotation(const SunPosition&)` returns R = atan2(sin z · sin(γ − 180°), cos z) in degrees; positive = west.
- **R4** `double tracking::target(double R, double minDeg, double maxDeg)` clamps R to [minDeg, maxDeg] (defaults −45 / +45). When the sun is lower than the range, the panel stays at the limit on the sun's side.
- **R5** `bool sun::isNight(const SunPosition&)` is true when elevation (90 − z) < 0.
- **R6** No heap allocation; one call takes < 1 ms on the ESP32 at 240 MHz (measured once on hardware and written in Progress notes).

## 3. Data

Fixture CSV columns: `utc_epoch,lat,lon,azimuth_deg,zenith_deg`.

## 4. API / logic

Header `lib/core/sun/sun_position.h`, `lib/core/sun/tracking.h`. Doubles throughout (the ESP32 FPU is single precision, but double is fast enough at 10 Hz and avoids precision loss in the time terms).

## 5. Screens

None.

## 6. Files to create or change

- `firmware/lib/core/sun/sun_position.{h,cpp}`, `firmware/lib/core/sun/tracking.{h,cpp}`
- `firmware/tools/make_spa_fixture.py`, `firmware/test/fixtures/spa_kerman.csv`
- `firmware/test/test_cycle_002_sun/test_main.cpp`

## 7. Edge cases

- Times around UTC midnight (Iran local 03:30) and year boundaries; leap years.
- Sun exactly at zenith or exactly on the axis plane (R = 0) — no division by zero (atan2 handles it).
- Morning: R negative (east); solar noon: R ≈ 0; afternoon: R positive.

## Acceptance criteria

- **Must happen:** R2 accuracy test passes; sign and clamp tests pass (R3, R4); night flag correct at sunrise/sunset ±1 min (R5).
- **Must not happen:** dependence on Arduino headers; heap use.
- **Done when:** `test_cycle_002*` passes on native and R6 timing is recorded.

## Documentation Impact

- none — core library only; architecture already names `lib/core/sun`.

## Open Questions

- none

## Decisions

- PSA+ instead of full NREL SPA on the device. Because PSA+ is ~0.01° accurate, a few dozen lines and fast; SPA is used only as the test reference. (from the proposal)
