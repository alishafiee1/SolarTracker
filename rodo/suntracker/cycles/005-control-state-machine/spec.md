# 005 — Control state machine and stall detection: technical spec

> Proposal section 05 (7-step loop). Uses 002 (target), 003 (angle), 004 (motion), 006 (wind flag).

## 1. Summary

`ControlStateMachine` decides each tick which mode is active and what `MotionController` should do, detects stalls by angle, latches faults and records events.

## 2. Rules

- **R1** Active state each tick, highest priority wins: `Fault > Wind > Manual > Night > Auto`. Changing state stops any move that the new state does not want.
- **R2** Auto: target from 002 (clamped). A move starts when |angle − target| > `moveThresholdDeg` (default 5°, settable 2–15°). The move ends when |angle − target| ≤ `stopToleranceDeg` (1°) or the angle passes the target.
- **R3** Night (elevation < 0): target 0° (stow), same move rules. Optional `preDawnMinutes` (default 0 = off): that many minutes before sunrise the target becomes `minDeg` (east end).
- **R4** Manual: commands `ManualEast`, `ManualWest`, `ManualStop` from the web. A manual move runs until `ManualStop`, the min/max angle, or the 120 s limit (004 R6). While a manual move runs, the web client must send a keep-alive at least every 3 s; otherwise the move stops (dead-man). Manual mode stays until the user selects Auto.
- **R5** Stall by angle: during `Running`, every `stallCheckMs` (default 5000) the angle must have changed ≥ 1° towards the target. Otherwise: stop, wait 10 s, retry. After 3 failed attempts in a row → `Fault::Stall`. A successful move resets the counter. (When module 009 is present, its current reading refines the decision.)
- **R6** Fault latch: in `Fault` no automatic or manual move runs until `ResetFault` from the web. Exception: if wind protection asks for stow while in `Fault::Stall` or `Fault::MoveTimeout`, exactly one stow attempt is made (proposal step 7). No exception for `AngleSensor`, `InterlockViolation` or `RelayStuck`.
- **R7** `Fault::AngleSensor` (003) or clock not set (007) → no movement at all; status explains why.
- **R8** `ResetFault` clears the fault and the stall counter and resumes the active mode on the next tick.
- **R9** Every state change and every fault → `IEventLog` (`StateChange from to`, `Fault type detail`, `FaultReset`).
- **R10** Watchdog loop guard: 3 watchdog resets within 10 min (from 001) → `Fault::ResetLoop`.

## 3. Data

Settings used: `moveThresholdDeg`, `stallCheckMs`, `preDawnMinutes`, `minDeg`, `maxDeg`, `mode` (persisted auto/manual).

## 4. API / logic

`class ControlStateMachine { void tick(nowMs, const Inputs&); void command(Command); Status status() const; }` where `Inputs { angleDeg, angleOk, calibrated, utc, clockValid, windStow, currentA, currentPresent }`.

## 5. Screens

Status texts (shown in 008): «ردیابی خورشید», «شب — حالت ایمن», «باد شدید — حالت ایمن», «حالت دستی», «خطا: گیرکردن بازو», «خطا: سنسور زاویه», «منتظر تأخیر ایمنی (n ثانیه)», «ساعت تنظیم نشده», «کالیبراسیون لازم است».

## 6. Files to create or change

- `firmware/lib/core/control/control_state_machine.{h,cpp}`, `firmware/lib/core/control/fault.h`
- `firmware/lib/core/control_loop.cpp` (wire inputs → state machine → motion)
- `firmware/test/test_cycle_005_control/test_main.cpp`

## 7. Edge cases

- Target jumps from −45 to 0 at sunset → one move, not oscillation.
- Angle noise around the threshold → no repeated starts (threshold 5° vs stop tolerance 1° gives hysteresis).
- Manual command while wind stow active → rejected with status «باد شدید — حالت ایمن».

## Acceptance criteria

- **Must happen:** proposal scenarios 1 (stall), 3 (night), 9 (fault reset) pass in the simulator; tracking error stays < 6° over a simulated sunny day (scenario 11, full run in 013).
- **Must not happen:** any move while in `Fault` except the R6 exception; manual move continuing without keep-alive.
- **Done when:** `test_cycle_005*` passes on native.

## Documentation Impact

- `rodo/suntracker/project/glossary.md` — add any new state names if they differ.

## Open Questions

- none

## Decisions

- Stop tolerance 1° (move starts at 5°). Because the proposal says "move until the target"; 1° gives hysteresis without hunting. (default, not asked)
- Dead-man keep-alive for manual moves (3 s). Because a phone that loses the hotspot mid-move must not leave the actuator running until the 120 s limit. (default, not asked)
