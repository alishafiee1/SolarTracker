# 004 — Safe actuator motion with direction interlock: technical spec

> Read with `design.md`. Proposal section 04 ("فرمان حرکت با قفل سخت‌افزاری جهت").

## 1. Summary

`MotionController` is the single owner of `IRelays`. It turns `move(East|West)` and `stop()` requests into a relay sequence that respects every timing interlock, in every mode.

## 2. Rules

- **R1** Only `MotionController` holds an `IRelays&`. States: `Idle`, `Selecting` (CH2 set, waiting to power), `Running` (CH1 on), `Settling` (CH1 off, waiting). Public API: `request(MotionCmd)`, `emergencyStop(reason)`, `tick(nowMs, angleDeg, currentA)`, `state()`, `direction()`, `lastMoveEndMs()`.
- **R2** Start of a move: CH1 is off → set CH2 to the requested direction → wait `relaySelectMs` (200 ms) → CH1 on.
- **R3** `stop()`: CH1 off in the same tick; CH2 untouched. Then `Settling` for at least `stopSettleMs` (default 3000 ms, settable 1000–10000).
- **R4** A move in the opposite direction of the last move may start only when all hold: settle time elapsed; current < `zeroCurrentA` (0.2 A) if a current sensor is present; angle changed < 0.5° during the last 1 s.
- **R5** Between the end of one move and the start of the next (any direction) at least `minMoveGapMs` (default 10000 ms) passes. A request that comes earlier is held and starts when allowed (latest request wins).
- **R6** A move longer than `maxMoveMs` (default 120000 ms) is stopped and reported as `Fault::MoveTimeout`.
- **R7** All callers (auto, manual, wind, night) go through the same API; there is no bypass. Wind stow is just the highest-priority request.
- **R8** CH2 can only be written while CH1 is off and the controller is not `Running`; any attempt otherwise is a programming error: CH1 off, `Fault::InterlockViolation`, event logged. Covered by a test that tries it.
- **R9** After boot the controller starts in `Settling` with a full `stopSettleMs`, so the first move is never immediate.
- **R10** `emergencyStop()` turns CH1 off immediately, regardless of any timer, and moves to `Settling`.
- **R11** Every move start and end produces an event (`MoveStart dir angle`, `MoveEnd reason angle duration`).

## 3. Data

Timing values come from `Settings` (007): `relaySelectMs` (fixed 200), `stopSettleMs`, `minMoveGapMs`, `maxMoveMs`, `zeroCurrentA`.

## 4. API / logic

`enum class Dir { East, West }; struct MotionCmd { enum { Move, Stop } kind; Dir dir; };` The controller is a pure state machine; `tick()` is called every 100 ms by `ControlLoop`.

## 5. Screens

None (state is shown by 008).

## 6. Files to create or change

- `firmware/lib/core/motion/motion_controller.{h,cpp}`, `firmware/lib/core/motion/safety_timing.h`
- `firmware/lib/sim/fake_relays.h` (records every write with a timestamp; asserts the interlock)
- `firmware/test/test_cycle_004_motion/test_main.cpp`

## 7. Edge cases

- Stop request during `Selecting` → CH1 never turns on; CH2 stays; go to `Idle` (no settle needed because nothing moved).
- Same-direction request while `Running` → ignored (keeps running).
- Requests flood (manual button spam) → R5 and "latest request wins".
- Reset during `Running` → hardware pull-ups and R9 make it safe.

## 8. Acceptance run

`rodo test 004`. Bench test on real relays with the actuators disconnected (proposal: "phase 1 priority: command safety") before any actuator is connected.

## Acceptance criteria

- **Must happen:** sequences R2, R3, R4, R5, R6, R9, R10 exactly as written; proposal scenario 13 passes in the simulator.
- **Must not happen:** CH2 changes while CH1 is on (R8); CH1 on within 200 ms after a CH2 change; a move starting less than 10 s after the previous one ended.
- **Done when:** `test_cycle_004*` passes; the relay bench test (direction LEDs/meters, no actuator) is recorded in Progress notes.

## Documentation Impact

- `rodo/suntracker/project/architecture.md` — confirm the timing defaults in `Settings` if they change.

## Open Questions

- none

## Decisions

- One owner for the relays (`MotionController`). Because the interlocks must hold in every mode. (MEMORY.md)
- Without the current sensor (module 009 removed), R4 uses only settle time + stable angle. Because the base system must be safe without optional modules. (default, not asked)
- "Latest request wins" for held requests. Because a queued old command (e.g. east after the user pressed west) would surprise the user. (default, not asked)
