# 017 — Panel design: technical spec

## 1. Summary

A clearer dashboard and better forms, within the small embedded budget (no framework, no web fonts).

## 2. Rules

- **R1** The dashboard draws an inline SVG of the panel at the current angle with a mark at the target angle and the limits (−45 … +45 by default, from the settings).
- **R2** The status text sits in a coloured badge: green = tracking/night, amber = waiting (safety delay, wind stow, manual), red = fault.
- **R3** A sticky header with the connection state; tab groups «وضعیت / کنترل / تنظیمات / ابزار»; a toast for every save and error instead of a line of text under the form.
- **R4** `GET /api/log` also returns `storedBytes` and `maxBytes`; the log tab shows the fill and the number of events, and a filter by type (faults, moves, system).
- **R5** The settings form marks changed fields and warns before leaving with unsaved changes; resetting a fault asks for confirmation.
- **R6** The gzipped panel stays below 30 KB.

## 3. Data

`storedBytes` (both log files) and `maxBytes` (131072) in the log response.

## 4. API / logic

`LittleFsEventLog::storedBytes()`; `webapi::Deps::logBytes` callback.

## 5. Screens

Dashboard, log tab, forms.

## 6. Files to create or change

- `firmware/web/*`
- `firmware/lib/core/webapi/web_api.{h,cpp}`
- `firmware/lib/drivers/littlefs_event_log.{h,cpp}`
- `firmware/test/test_cycle_017_ui/test_main.cpp` (log response, size limit)

## 7. Edge cases

- Very narrow phones (320 px): tabs scroll horizontally, nothing overflows.
- Dark mode follows the phone.

## Acceptance criteria

- **Must happen:** the SVG moves with the angle; badges match the state; the log tab shows the fill.
- **Must not happen:** horizontal page scroll on a phone; a panel above 30 KB gzipped.
- **Done when:** host tests pass and the panel looks right in a browser at phone width and on the desktop.

## Documentation Impact

- none

## Open Questions

- none

## Decisions

- No chart library. Because an inline SVG is enough and the panel is part of the firmware image.
