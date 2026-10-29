# 009 — Current protection (module B2 + C2, requested by the client): technical spec

## 1. Summary

Read SCT-013 on GPIO 32, compute RMS, and use it to detect stall, limit reached, relay stuck, wire break and overcurrent.

## 2. Rules

- **R1** Sample ADC1_CH4 at ≥ 2 kHz for 200 ms (10 mains cycles at 50 Hz) every 500 ms, remove the measured DC bias (~1.65 V), compute RMS, multiply by `currentFactor` (setting, calibrated against a clamp meter). Sampling must not block the control tick for more than 5 ms (use the ADC continuous/DMA mode or a separate low-priority task).
- **R2** Stall: while `Running`, current > `stallCurrentA` (default 4.5 A) and angle unchanged (005 R5) → stall attempt (same retry policy).
- **R3** Limit vs wire break: while `Running`, current < `zeroCurrentA` (0.2 A) for 1 s after the first 500 ms → if the angle is within 5° of the end in the moving direction (`maxDeg`/`minDeg` ± mechanical margin) → `LimitReached` (event, no fault, stop); else → `Fault::WireBreak`.
- **R4** Relay stuck: current > 0.3 A for > 1 s while CH1 is commanded off → `Fault::RelayStuck`, event, alert on the panel (cannot be fixed by software).
- **R5** Overcurrent: current > `maxCurrentA` (default 5 A) for > 300 ms while running → emergency stop + `Fault::Overcurrent`.
- **R6** With the module present, 004 R4 uses the current condition; `currentPresent` is a setting (default true; false only if the sensor is not fitted), and every rule here is skipped when false.

## 3. Data

Settings: `currentPresent`, `currentFactor`, `stallCurrentA`, `maxCurrentA`, `zeroCurrentA`.

## 4. API / logic

`Sct013Current : ICurrentSensor`; `CurrentSupervisor` in core consumes `rmsAmps()` and motion state.

## 5. Screens

Dashboard shows «جریان جک: n A». Fault texts: «رله چسبیده — برق جک را دستی قطع کنید», «قطع سیم جک», «اضافه جریان».

## 6. Files to create or change

- `firmware/lib/core/current/current_supervisor.{h,cpp}`
- `firmware/lib/drivers/sct013_current.{h,cpp}`
- `firmware/lib/core/control/` (hook stall refinement)
- `firmware/test/test_cycle_009_current/test_main.cpp`

## 7. Edge cases

- Inrush at motor start (first 300 ms) must not trip overcurrent.
- ADC noise at zero current (~0.05 A) stays below thresholds.

## Acceptance criteria

- **Must happen:** proposal scenarios 6 (relay fault) and 7 (end of stroke without false fault) in the simulator; RMS within ±5 % of a clamp meter on the bench.
- **Must not happen:** false stall on motor start; the module affecting behaviour when `currentPresent` is false.
- **Done when:** `test_cycle_009*` passes and the bench calibration is recorded.

## Documentation Impact

- `rodo/suntracker/project/architecture.md` — current settings fields.

## Open Questions


## Decisions

- `currentPresent` defaults to **false**, not true as first planned. Because the board under development has no current transformer, and with the module on a missing sensor reads 0 A while powered, which is a wire break. The panel setting «ماژول جریان نصب است» turns it on at delivery (it is part of the delivery checklist).
- The first 300 ms after switching on are not judged (inrush), and a jam is judged by the overcurrent limit first: a jam that draws more than `maxCurrentA` (5 A) is stopped at once as `Overcurrent`; a jam between `stallCurrentA` (4.5 A) and 5 A takes the normal three tries and becomes `Stall`. Because real jam currents are not known yet; both thresholds are settings.
- No current at all (below `zeroCurrentA` for 1 s) is `LimitReached` only within 5 degrees of the configured min or max angle; elsewhere it is `WireBreak`. A jack whose limit switch is set well inside the software range is therefore reported as a wire break, which is the right hint: set the range to the mechanics. After a limit the controller does not push the same way again until the panel has left that end by 2 degrees.
- A stuck relay cannot be fixed by software: CH1 is commanded off, the fault is latched and the panel says to cut the supply by hand.
- Sampling runs in its own low-priority task on core 0 (200 ms of ADC reads every 500 ms), so the control tick never waits for it (measured: 9.98 ticks per second with the task running).
- Client wants all optional modules (answer of 2026-10-29). Because the user confirmed the full scope.
- Optional module behind a `currentPresent` setting. Because the base system must work and stay safe without it. (MEMORY.md)
