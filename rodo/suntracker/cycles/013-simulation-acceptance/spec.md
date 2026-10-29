# 013 — Simulator and acceptance scenarios: technical spec

## 1. Summary

A host-side simulator (fake HAL + physical models + simulated time) and a test suite that runs the proposal's 13 acceptance scenarios, plus a field checklist for delivery.

## 2. Rules

- **R1** `lib/sim` models: actuator (speed `actuatorDegPerS`, default 0.6 °/s; coast-down 0.3 s; internal limits at −50/+50°; injectable stall and wire break; current draw 3 A running, 0 A stopped, configurable inrush), wind (speed profile vs time), accelerometer (orientation + Gaussian noise σ = 0.3°), RTC (settable, lost-power flag), relays (records writes; asserts the hardware interlock), supply and current sensors. Simulated time advances in 10 ms steps; one simulated day runs in < 10 s on a laptop.
- **R2** One Unity test per proposal scenario 1–13 (`scenario_01_stall` … `scenario_13_direction_change`), each run with 3 different random seeds. Scenarios that need optional modules (6, 7, 10) are skipped with a clear message when the module is disabled.
- **R3** Full-day tracking for Kerman on 2027-03-20, 2027-06-21, 2027-09-23, 2027-12-21: max |angle − target| < 6° while the sun is within the range (scenario 11); the test prints the number of moves per day.
- **R4** 72 h soak with a realistic wind profile and two power cuts: no watchdog reset, no false fault, log size stays ≤ 128 KB (scenario 12, simulated).
- **R5** `docs/acceptance-checklist.md`: a table of the 13 scenarios with setup steps on real hardware, expected result, and three result columns (run 1–3, date, signature), plus the relay bench test from 004 and the one-day log check.

## 3. Data

None persisted; the simulator can dump a CSV trace (`time, target, angle, relays, wind`) for debugging.

## 4. API / logic

`Sim world(seed); world.setDate(...); world.wind(profile); world.run(hours);` wired to the real `ControlLoop`.

## 5. Screens

None.

## 6. Files to create or change

- `firmware/lib/sim/world.{h,cpp}`, `actuator_model.{h,cpp}`, `wind_profile.h`
- `firmware/test/test_cycle_013_acceptance/test_main.cpp`
- `docs/acceptance-checklist.md`

## 7. Edge cases

- Simulator bugs can hide real bugs → models are kept simple and each model has its own unit test.

## Acceptance criteria

- **Must happen:** all software-testable scenarios pass 3/3 seeds; R3 and R4 pass.
- **Must not happen:** tests that depend on wall-clock time or are flaky.
- **Done when:** `test_cycle_013*` passes and the checklist file exists and is linked from README.

## Documentation Impact

- `README.md` — link to the acceptance checklist.
- `rodo/suntracker/project/architecture.md` — confirm simulator paths.

## Open Questions

- Real actuator speed (°/s)? — default if no answer: 0.6 °/s; update after the first bench measurement.

## Decisions

- The simulator steps in 100 ms (the control period), not 10 ms. Because the firmware only reacts every 100 ms; smaller steps would only add cost. A simulated day runs in about 1 s.
- Calibration in the simulator is done with no sensor noise, the 0.3 degree noise models wind and vibration during operation. Because the calibration itself requires a still panel (it rejects samples that vary by more than 0.3 degrees).
- Scenarios 6, 7 and 10 are reported as ignored with a reason until modules 009 and 010 exist. Because their expected behaviour (current-based fault, end of stroke, update) cannot happen without those modules.
- Scenario 11 measures the error only after the panel has first come within 5 degrees of the sun target after sunrise (it has to travel from the night position), and only while the sun is up. The time to get there is printed.
- Scenario tests run against the real `ControlLoop`, not a copy. Because the point is to test the code that ships.
