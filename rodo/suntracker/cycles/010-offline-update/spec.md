# 010 — Offline update from the web (module C4, requested by the client): technical spec

## 1. Summary

Upload a firmware image (which also contains the web panel) over the hotspot, validate it, boot it, and roll back automatically if it does not confirm itself.

## 2. Rules

- **R1** Page "به‌روزرسانی" and `POST /api/update` (multipart, login required) accept a `.bin` ≤ the OTA slot size. Progress is shown in percent.
- **R2** During the update: `ControlStateMachine` enters `Updating` (CH1 off via `emergencyStop`, no moves), the event `UpdateStart` is logged, and wind protection is suspended for at most 3 min (an update that takes longer is aborted).
- **R3** The image is written to the inactive OTA slot with the `Update` library; the ESP image header, size and the appended SHA-256 must be valid and the project magic string `SUNTRACKER` must be present in the app description; otherwise abort with «فایل نامعتبر است» and keep the current slot.
- **R4** Rollback: after booting a new image a `pendingVerify` flag is set in NVS. The new firmware marks itself valid (`esp_ota_mark_app_valid_cancel_rollback()` or clearing the flag) only after a self-test: settings loaded, RTC and accelerometer respond, control loop ran 60 s, web server up. If the flag is still set after 3 boots or a watchdog reset within those 60 s, the bootloader/app switches back to the previous slot and reboots. Event `UpdateOk ver` / `UpdateRolledBack ver`.
- **R5** The web panel files are inside the same image (008 R2), so one file updates both.

## 3. Data

NVS: `pendingVerify`, `bootTries`, `prevVersion`.

## 4. API / logic

`OtaUpdate` driver wraps `Update` and `esp_ota_*`.

## 5. Screens

Texts: «در حال ارسال: n٪», «فایل نامعتبر است», «نصب شد؛ دستگاه دوباره راه‌اندازی می‌شود», «نسخه جدید کار نکرد؛ به نسخه قبلی برگشت».

## 6. Files to create or change

- `firmware/lib/drivers/ota_update.{h,cpp}`
- `firmware/lib/web/api_handlers.cpp` (update endpoint), `firmware/web/` (update page)
- `firmware/scripts/version.py` (embed magic string and version)
- `firmware/test/test_cycle_010_ota/test_main.cpp` (validation logic on native)

## 7. Edge cases

- Phone disconnects mid-upload → abort, current slot stays, control resumes.
- Power loss mid-write → the inactive slot is incomplete and never selected.

## Acceptance criteria

- **Must happen:** proposal scenario 10 (good file installs, broken file is rejected and the old version keeps running), 3 times on hardware; a deliberately crashing image rolls back.
- **Must not happen:** actuator movement during an update.
- **Done when:** native validation tests pass and the hardware runs are recorded.

## Documentation Impact

- `README.md` — how to build and upload an update file.

## Open Questions

- Does the Arduino-ESP32 bootloader in the chosen platform version have app rollback enabled? — default if no answer: use the NVS `pendingVerify` + boot-counter approach in R4, which works without bootloader support.

## Decisions

- Client wants all optional modules (answer of 2026-10-29). Because the user confirmed the full scope.
- Self-test before confirming the image. Because a bad image that boots but breaks control must still roll back. (default, not asked)
