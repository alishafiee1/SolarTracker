# 003 — Panel angle sensing and calibration: technical spec

## 1. Summary

Read the on-board MPU6050, turn the gravity vector into a signed rotation angle using a two-step calibration, smooth it, and detect sensor faults.

## 2. Rules

- **R1** Driver `Mpu6050Accel : IAccel` on I2C address 0x69 (AD0 high), ±2 g range, DLPF ~21 Hz. At init and every 10 s it checks `WHO_AM_I` (0x68 expected value). Reads raw accel and converts to g.
- **R2** Calibration: store g0 (panel horizontal) and g1 (panel turned a few degrees east). Rotation axis a = normalize(g0 × g1). Current angle θ = signed angle from g0 to the projection of g onto the plane ⟂ a, with the sign chosen so that the g1 side (east) is **negative** (R positive = west, see glossary).
- **R3** Calibration is rejected with a clear reason when: the angle between g0 and g1 is < 3° or > 30°; |g0| or |g1| is outside 0.9–1.1 g; or the samples during capture vary more than 0.3° (panel still moving). Each capture averages 50 samples (5 s).
- **R4** Sampling at 10 Hz (every tick); the control logic uses the moving average of the last 5 valid samples.
- **R5** Sensor fault within 1 s when: I2C NACK / timeout, wrong `WHO_AM_I`, |g| outside 0.8–1.2 g for 5 consecutive samples, or NaN. The fault is reported as `Fault::AngleSensor`; the state machine (005) stops all motion.
- **R6** Without a valid stored calibration, `AngleProvider::calibrated()` is false and automatic movement is not allowed; status shows "calibration needed".
- **R7** The calibration vectors are saved through `ISettingsStore` (cycle 007 provides NVS; until then the fake store is used).

## 3. Data

`AngleCalibration { float g0[3]; float g1[3]; uint32_t createdUtc; }` stored in settings.

## 4. API / logic

`class AngleProvider { void sample(); float angleDeg() const; bool ok() const; void clearFault(); bool calibrated() const; bool beginCapture(CalStep); bool capturing() const; CalResult captureResult() const; bool saveTo(ISettingsStore&) const; bool loadFrom(ISettingsStore&); }`. A capture is non-blocking: `beginCapture()`, keep calling `sample()` (50 ticks = 5 s), poll `captureResult()` (`Pending` until done).

## 5. Screens

None here (calibration wizard in 008). Error texts the web panel will show: «سنسور زاویه پاسخ نمی‌دهد», «کالیبراسیون نامعتبر: پنل در حال حرکت بود», «کالیبراسیون نامعتبر: زاویه دو مرحله خیلی کم است».

## 6. Files to create or change

- `firmware/lib/core/angle/angle_provider.{h,cpp}`, `firmware/lib/core/angle/vec3.h`
- `firmware/lib/drivers/mpu6050_accel.{h,cpp}`
- `firmware/lib/sim/fake_accel.h` (orientation + noise)
- `firmware/test/test_cycle_003_angle/test_main.cpp`

## 7. Edge cases

- Box mounted upside down or rotated 90° around any axis → still correct after calibration.
- Wind vibration: noise ±0.5° must not trigger movement (moving average + threshold in 005).
- Angle near ±90° is out of range for this tracker; values beyond ±60° are reported but flagged implausible (fault after 5 samples).

## Acceptance criteria

- **Must happen:** for 6 random mounting orientations, computed angle within 0.5° of truth over −45…+45° (R2); invalid calibrations rejected (R3); fault within 1 s of unplugging (R5).
- **Must not happen:** movement allowed while uncalibrated (R6).
- **Done when:** `test_cycle_003*` passes on native and one hardware check (tilt the board by a known angle) is recorded.

## Documentation Impact

- none — the architecture already describes the sensor; glossary has «کالیبراسیون دو مرحله‌ای».

## Open Questions

- none

## Decisions

- A capture is non-blocking (begin, poll). Because a 5 s blocking call from a web handler would stall the single control thread. (default, not asked)
- Every kind of invalid sample (failed read, NaN, |g| outside 0.8–1.2, implausible angle) counts the same: 5 in a row (0.5 s) latch the fault, which stays until `clearFault()` (the state machine's fault reset). Because it keeps the rule simple and a recovering sensor must not resume automatic motion by itself. (default, not asked)
- A missing sensor is retried once per second, not every tick. Because each failed I2C attempt prints an error and the control tick must stay fast. (default, not asked)
- `WHO_AM_I` must be 0x68 as the spec says; some GY-521 clones answer another value and would be reported as a sensor fault. If the real module does that, allow its value in `Mpu6050Accel::kWhoAmIExpected`. (default, not asked)
- East = negative angle. Because the proposal defines R positive towards west.
- Reject calibration if the panel moved during capture. Because a bad calibration silently breaks tracking and stall detection. (default, not asked)
