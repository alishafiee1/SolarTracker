# 015 — Wi-Fi scan: technical spec

## 1. Summary

The panel can list the networks in range so the installer picks the site Wi-Fi instead of typing it.

## 2. Rules

- **R1** `POST /api/wifi/scan` starts an asynchronous scan (`WiFi.scanNetworks(true)`) and returns at once; the control loop is never blocked. A second request while one runs is answered with the running state.
- **R2** `GET /api/wifi/scan` returns `{state: "idle|scanning|done", networks: [{ssid, rssi, secure}]}`; the list holds at most 20 entries, sorted by signal (strongest first), hidden networks (empty SSID) removed, and one entry per SSID (the strongest).
- **R3** Both endpoints need a login. The result is not stored beyond the next scan.
- **R4** The network page shows a scan button, the list (signal bars, lock icon) and fills the SSID field on a tap.
- **R5** The ordering and de-duplication is a pure function `tidyScan(entries)` tested on the host.

## 3. Data

None persisted.

## 4. API / logic

`webapi::Deps::startWifiScan`, `webapi::Deps::wifiScanResult` callbacks; the driver fills them.

## 5. Screens

Text: «جستجوی شبکه‌ها», «در حال جستجو…», «شبکه‌ای پیدا نشد», signal «ضعیف / متوسط / خوب».

## 6. Files to create or change

- `firmware/lib/core/net/net_logic.{h,cpp}` — `tidyScan`.
- `firmware/lib/core/webapi/web_api.{h,cpp}` — endpoints.
- `firmware/lib/drivers/site_net.{h,cpp}` — scan start and result.
- `firmware/web/*` — the page.
- `firmware/test/test_cycle_015_scan/test_main.cpp`

## 7. Edge cases

- The scan makes the radio leave the hotspot channel for a moment; a phone may reconnect. Accepted and written in the page text.
- 2.4 GHz only: the ESP32 does not see 5 GHz networks. Say so in the page.

## Acceptance criteria

- **Must happen:** the list shows the site network with its signal.
- **Must not happen:** the control loop waiting for the scan.
- **Done when:** host tests pass and the scan works on the board.

## Documentation Impact

- none

## Open Questions

- none

## Decisions

- Asynchronous scan with a poll. Because a blocking scan takes about 2 seconds.
