# 012 — Log CSV export (optional module C6): technical spec

## 1. Summary

Stream the rotating event log as one CSV file from the web panel.

## 2. Rules

- **R1** `GET /api/log.csv` (login required); the Log page has a «دانلود CSV» button. File name `suntracker-<XXXX>-<yyyy-mm-dd>.csv`.
- **R2** Output is streamed (chunked response) from the older log file to the newer one; RAM use < 4 KB regardless of log size.
- **R3** Columns: `local_time,utc_time,type,detail` (ISO 8601; local = UTC + `tzOffsetMin`).
- **R4** UTF-8 with BOM and CRLF line ends so Excel shows Persian text correctly; fields with commas or quotes are quoted.
- **R5** Log writes are paused (buffered) while the file is streamed so rotation cannot cut the file.

## 3. Data

Reads the files from 007 R6.

## 4. API / logic

Chunked response callback reading the LittleFS files.

## 5. Screens

Button «دانلود CSV» on the Log page.

## 6. Files to create or change

- `firmware/lib/web/api_handlers.cpp`, `firmware/lib/web/csv_export.{h,cpp}`, `firmware/web/` (button)
- `firmware/test/test_cycle_012_csv/test_main.cpp` (formatting and escaping on native)

## 7. Edge cases

- Empty log → header only.
- Download interrupted → no side effects; writes resume.

## Acceptance criteria

- **Must happen:** a 128 KB log downloads completely and opens in Excel with correct Persian text.
- **Must not happen:** control tick delays > 20 ms during the download.
- **Done when:** native tests pass and a download from a phone is recorded.

## Documentation Impact

- none

## Open Questions

- Did the client keep this optional module? — default if no answer: implement after the base cycles.

## Decisions

- Login required for the download. Because the log reveals the operating pattern of the site. (default, not asked)
