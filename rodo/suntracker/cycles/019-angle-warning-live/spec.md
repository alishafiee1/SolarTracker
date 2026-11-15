# 019 — Out-of-range angle is a warning, not a sensor fault: technical spec

## 1. Summary

Cycle 003 R5 latched `Fault::AngleSensor` for every invalid sample, including a valid reading whose angle is beyond +-60 degrees. An installer who turns the box upside down therefore had to reset a fault every time. Now only a sensor that gives no data or bad data is a fault; an angle outside the normal range is a non-latched warning that blocks automatic movement.

## 2. Rules

- **R1** Fault (unchanged, latched until reset): read failure, NaN, or corrected `|g|` outside 0.8..1.2, for 5 consecutive samples.
- **R2** Warning (new, not latched): with a calibration, a valid reading whose angle is undefined (gravity along the rotation axis) or `|angle| > 60` degrees. It is set after 5 consecutive such samples and cleared after 5 consecutive normal samples (0.5 s each way, no flicker).
- **R3** While the warning is on, `angleDeg()` is the live angle (not the 5-sample average), and the moving average restarts when the angle returns to the normal range.
- **R4** While the warning is on the state machine makes no automatic movement (Auto, Night and Wind stow all stop, like `NeedCalibration`): status text `AngleOutOfRange` («هشدار: زاویه خارج از محدوده است — حرکت خودکار متوقف»). Manual movement stays allowed. Faults keep their priority.
- **R5** `/api/status` adds `angleWarning` (bool) and `liveAngleDeg` (number, `null` without a calibration or while the sensor is faulty). The `statusId` of the new text is 15.
- **R6** Panel: the dashboard shows an amber warning line while `angleWarning`; the calibration page shows the live angle with "در محدوده" or "خارج از محدوده".

## 3. Data

None stored.

## 4. API / logic

`AngleProvider`: `outOfRange()`, `liveAngle(float&)`. `Inputs::angleOutOfRange`, `Status::angleWarning`, `StatusText::AngleOutOfRange`.

## 5. Screens

Dashboard: «هشدار: زاویهٔ دستگاه خارج از محدودهٔ عادی است. حرکت خودکار متوقف است؛ دستگاه را در وضعیت نصب بگذارید.» Calibration: «زاویهٔ زنده: ‎−۸٫۴° (در محدوده)».

## 6. Files to create or change

- `firmware/lib/core/angle/angle_provider.{h,cpp}`
- `firmware/lib/core/control/control_state_machine.{h,cpp}`
- `firmware/lib/core/webapi/web_api.cpp`, `firmware/src/main.cpp`
- `firmware/web/{index.html,app.js}`
- `firmware/test/test_cycle_003_angle`, `test_cycle_005_control`, `test_cycle_008_web`

## 7. Edge cases

- Sensor unplugged while the warning is on: the data check fails and becomes a fault (R1 wins).
- A latched fault from earlier stays until reset; the warning is independent of it.
- No calibration: no warning (the status already says `NeedCalibration`).

## Acceptance criteria

- **Must happen:** upside-down reading gives a warning in 0.5 s and no fault (R2); no automatic move while it lasts (R4); it clears 0.5 s after the angle is normal again (R2); an unplugged or NaN sensor still latches the fault (R1).
- **Must not happen:** a latched fault from an angle beyond +-60 degrees; manual movement blocked by the warning.
- **Done when:** `test_cycle_003*`, `005*`, `008*` pass and the whole host suite stays green.

## Documentation Impact

- none — cycle 003 is historical; this card records the new rule.

## Open Questions

- none

## Decisions

- Wind stow also stops while the angle is out of range. Because the stow target depends on the same angle, and the state machine already refuses it without a calibration.
- Manual movement stays allowed. Because the installer needs it to bring the panel back, and the jack's own limit switches protect the ends.
