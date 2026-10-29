# 011 — Site Wi-Fi and Bale notifications (optional module C5): technical spec

## 1. Summary

AP+STA Wi-Fi, NTP time sync and fault notifications through a Bale bot, all optional and never blocking control.

## 2. Rules

- **R1** `WIFI_AP_STA`: the hotspot stays up; STA connects to the stored site SSID with exponential backoff (5 s → 5 min). STA failure never affects control or the AP.
- **R2** When STA has internet: NTP (`pool.ntp.org`, then `time.google.com`) at boot and every 24 h; if |NTP − RTC| > 2 s, set the RTC (007 R4) and log `ClockSet ntp`.
- **R3** Bale: on every fault and on wind stow, send `"<device>: <Persian fault text> — <local time>"` to the stored chat id via the Bale Bot API (`https://tapi.bale.ai/bot<token>/sendMessage`, HTTPS). Queue up to 20 messages in RAM while offline; at most 1 message per 10 s; duplicates of the same fault within 10 min are merged.
- **R4** Site SSID/password and bot token/chat id are set in the web panel (login required), stored only in NVS, and never returned by any API (the panel shows "set"/"not set"). A "send test message" button exists.
- **R5** The panel is not exposed to the internet: no UPnP, no port forwarding; the server listens on the AP and the local STA network only.
- **R6** Network work runs in its own low-priority task; HTTPS calls have a 10 s timeout and never run in the control tick.

## 3. Data

NVS: `staSsid`, `staPassword`, `baleToken`, `baleChatId`, `notifyEnabled`.

## 4. API / logic

`POST /api/network`, `POST /api/notify`, `POST /api/notify/test`; status adds `staConnected`, `staIp`, `lastNotifyOk`.

## 5. Screens

Network page: SSID field, password field (write-only), connection state; notification page: token (write-only), chat id, test button. Texts: «به WiFi محل وصل شد: <ip>», «اتصال به WiFi محل ناموفق», «پیام آزمایشی فرستاده شد».

## 6. Files to create or change

- `firmware/lib/net/site_wifi.{h,cpp}`, `firmware/lib/net/ntp_sync.{h,cpp}`, `firmware/lib/net/bale_notifier.{h,cpp}`
- `firmware/lib/web/api_handlers.cpp`, `firmware/web/` (network and notify pages)
- `firmware/test/test_cycle_011_net/test_main.cpp` (queue, merge and rate-limit logic on native)

## 7. Edge cases

- Wrong token → the API answers 401; show «توکن ربات نامعتبر است», stop retrying until changed.
- AP and STA on different channels: the AP follows the STA channel; phones reconnect automatically.

## Acceptance criteria

- **Must happen:** a fault produces a Bale message within 30 s when online; NTP corrects a 1 min RTC error; turning off the site router does not affect tracking (scenario 8 variant).
- **Must not happen:** secrets in API responses, logs or events.
- **Done when:** native tests pass and an on-site test with a real router and bot is recorded.

## Documentation Impact

- `README.md` — how to create the Bale bot and get the chat id.

## Open Questions

- Did the client keep this optional module? — default if no answer: implement last.
- Confirm the Bale Bot API base URL at implementation time. — default: `https://tapi.bale.ai/bot<token>/`.

## Decisions

- Keep the AP always on in AP+STA mode. Because the proposal promises the hotspot stays available when site Wi-Fi fails.
