# 018 — Six-face offset calibration of the angle sensor: technical spec

## 1. Summary

A cheap accelerometer has a per-axis offset and gain error (measured on the user's board: Z reads +1.114 g face up and -0.906 g face down, i.e. offset +0.104 g, gain 1.01). The old `|g|` check (0.9..1.1) therefore rejected the angle calibration in some orientations. This card measures all six faces, stores a per-axis offset and gain, and corrects every reading before it is used.

## 2. Rules

- **R1** A face capture averages 50 raw samples (5 s). It is accepted only if the samples vary at most 0.3 degrees (as R3 of cycle 003), the raw `|mean|` is 0.7..1.3 g, and the device stands on a face: the dominant axis has `|v| >= 0.7` and the other two each `|v| <= 0.35`. Otherwise the result is `bad-face` (or `moving`, `bad-gravity`, `sensor-fault`).
- **R2** The face is detected automatically: index `f = 2*axis + (negative ? 1 : 0)` (0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z). Capturing a face again overwrites the earlier capture. `faces` is a 6-bit mask of captured faces.
- **R3** Save needs all six faces. Per axis k with p = reading when +k is up and n = reading when -k is up: `offset_k = (p+n)/2`, `scale_k = (p-n)/2`. Rejected as `bad-gravity` if any scale is outside 0.8..1.2.
- **R4** Every raw reading is corrected `g' = (g - offset) / scale` before the validity checks (live window 0.8..1.2, capture window 0.9..1.1) and before the angle maths. Without saved offsets `offset = 0` and `scale = 1`.
- **R5** Saving offsets invalidates the angle calibration (the reference vectors were taken in the old frame): `calibrated()` becomes false and the stored `angle_cal` blob is overwritten with zeros. `reset` clears the offsets the same way.
- **R6** Offsets and scales are stored as blob `angle_off` and loaded at boot before the angle calibration.
- **R7** API (login required): `POST /api/offsets/capture` starts a capture; `GET /api/offsets` returns `{capturing, result, faces, saved, offset[3], scale[3]}`; `POST /api/offsets/save`; `POST /api/offsets/reset` (also clears captured faces). `/api/status` `gravity` stays the RAW reading, so the panel can show the live face.
- **R8** Panel: card "آفست‌گیری سنسور" above the angle calibration: an unfolded cube (6 squares labelled X+ X- Y+ Y- Z+ Z-), the current face highlighted yellow from the live reading, captured faces green, buttons capture / save / reset. The angle calibration error `bad-gravity` explains that the offset step should be done first.

## 3. Data

`AngleOffsets { float offset[3]; float scale[3]; }` in blob `angle_off` (24 bytes, valid when every scale is 0.8..1.2).

## 4. API / logic

`AngleProvider`: `beginFaceCapture()`, `faceMask()`, `saveOffsets(store)`, `resetOffsets(store)`, `offsets()`, `lastGravityRaw()`. Capture result reuses `CalResult` plus `BadFace`.

## 5. Screens

Messages: «روی یک وجه صاف بگذارید» (bad-face), «دستگاه حرکت کرد»، «ثبت شد»، «ابتدا هر شش وجه را ثبت کنید»، «آفست‌ها ذخیره شد؛ کالیبراسیون زاویه را دوباره انجام دهید».

## 6. Files to create or change

- `firmware/lib/core/angle/angle_provider.{h,cpp}`
- `firmware/lib/core/webapi/web_api.{h,cpp}`
- `firmware/src/main.cpp` (nothing if `loadFrom` loads both)
- `firmware/web/{index.html,app.js,style.css}`
- `firmware/test/test_cycle_003_angle/test_main.cpp`, `firmware/test/test_cycle_008_web/test_main.cpp`

## 7. Edge cases

- Device tilted between two faces (e.g. 45 degrees) → `bad-face`, nothing stored.
- Capturing the same face twice → the second capture replaces the first.
- Offsets saved while an angle calibration exists → that calibration is dropped and the panel asks for it again.

## Acceptance criteria

- **Must happen:** with a simulated sensor (offset up to 0.15 g, gain 0.95..1.05) the six captures give offsets within 0.005 g (R3); after saving, the angle calibration is accepted in all six faces (R4); the angle error at +-45 degrees drops below 0.5 degrees (R4).
- **Must not happen:** offsets saved with fewer than six faces (R3); a stale angle calibration surviving a change of offsets (R5).
- **Done when:** `test_cycle_003*` and `test_cycle_008*` pass on native, and on the board all six faces were captured and the angle calibration was accepted.

## Documentation Impact

- `rodo/suntracker/project/architecture.md` — one line in "Main data" for the stored sensor offsets.

## Open Questions

- none

## Decisions

- The correction includes the gain, not only the offset. Because the six faces give the gain for free and the user's board has a 1 % gain error on Z.
- Face detection is automatic (dominant axis). Because the user does not know the printed axis names and a wrong button press would store garbage.
- Offsets invalidate the angle calibration. Because the stored reference vectors belong to the old correction and the angle would silently be wrong.
- `/api/status` `gravity` stays raw. Because the face detection on the panel must work before any offset exists.
