# 008 — Web panel on the hotspot: technical spec

## 1. Summary

Wi-Fi access point, async web server, embedded Persian RTL single-page panel and a small JSON REST API protected by a settings password.

## 2. Rules

- **R1** AP SSID `SunTracker-XXXX` (last 4 hex digits of the MAC), WPA2 password ≥ 8 chars: the same default on every device, changeable in Settings. IP 192.168.4.1, max 4 clients. The AP channel is fixed (default 6).
- **R2** ESPAsyncWebServer on port 80. The panel (HTML/CSS/JS, no external CDN, Vazirmatn font subset or system font) is built from `firmware/web/`, gzipped and embedded as byte arrays by `scripts/embed_web.py` at build time. Total ≤ 150 KB gzipped. RTL, Persian digits in the UI.
- **R3** `GET /api/status` (no login) returns JSON: `angleDeg, targetDeg, windKmh, currentA|null, mode, state, stateText, fault|null, waitingS, clockValid, calibrated, utc, tzOffsetMin, fwVersion`. The panel polls it every 1 s.
- **R4** Settings password (separate from the AP password) protects every endpoint that changes something. `POST /api/login {password}` → session cookie (random 128-bit token, HttpOnly, SameSite=Strict, 30 min idle timeout, max 4 sessions). Stored as salted SHA-256. After 5 wrong attempts in 1 min, logins are refused for 1 min. Every device ships with the same default settings password (kept out of the repo and given to the client at delivery); it can be changed in Settings, and the panel shows «رمز پیش‌فرض را عوض کنید» until it is changed.
- **R5** Pages: Dashboard (R3 values), Manual (East / Stop / West, shows safety wait), Settings (fields from 007 with ranges), Location (city list or "lat, lon" paste), Clock (sync from phone: sends `Date.now()`, or manual date/time), Calibration wizard (step 1 horizontal, step 2 east, shows result or the reason it was rejected), Log (latest 100 events, local time), Fault reset.
- **R6** Endpoints (login required): `POST /api/mode {auto|manual}`, `POST /api/manual {east|west|stop}`, `POST /api/manual/keepalive` (every 1 s while a button is active; 005 R4), `GET/POST /api/settings`, `POST /api/location`, `POST /api/clock {utc}`, `POST /api/calibrate/{horizontal|east|save|cancel}`, `POST /api/fault/reset`, `GET /api/log?n=100`, `POST /api/password`. Errors return `{error, field?}` with the Persian message.
- **R7** Handlers never call core objects directly: they push `Command`s into a queue consumed by `ControlLoop` and read the latest `Status` snapshot (copied under a short critical section). No handler blocks longer than 20 ms.
- **R8** If Wi-Fi or the web server fails, control continues (proposal scenario 8); the web task is restarted after 30 s.

## 3. Data

Uses `Settings` (007) and `Status`. Adds `apPassword`, `settingsPasswordHash`, `settingsPasswordSalt`, `passwordIsDefault`.

## 4. API / logic

See R6. JSON via ArduinoJson. Command queue: FreeRTOS queue of 8 `Command`s.

## 5. Screens

Exact texts include: «اتصال برقرار است», «اتصال قطع شد — در حال تلاش دوباره», «رمز اشتباه است», «تلاش زیاد؛ یک دقیقه صبر کنید», «منتظر تأخیر ایمنی: n ثانیه», «تنظیمات ذخیره شد», «مقدار نامعتبر: …», «کالیبراسیون ذخیره شد», «خطا ریست شد».

## 6. Files to create or change

- `firmware/lib/web/web_server.{h,cpp}`, `firmware/lib/web/api_handlers.{h,cpp}`, `firmware/lib/web/auth.{h,cpp}`
- `firmware/web/index.html`, `firmware/web/app.js`, `firmware/web/style.css`
- `firmware/scripts/embed_web.py`
- `firmware/src/main.cpp` (start AP and server after control)
- `firmware/test/test_cycle_008_web/test_main.cpp` (auth, JSON shaping, command mapping on native)

## 7. Edge cases

- Two phones at once: both see status; the last manual command wins; both sessions valid.
- Phone sleeps while holding "East": keep-alive stops → move stops (005 R4).
- Captive-portal popups on Android/iOS: answer the common probe URLs with a redirect to `/`.

## Acceptance criteria

- **Must happen:** every page works on Android Chrome and iOS Safari without internet; protected endpoints return 401 without a session; scenario 8 (Wi-Fi off) and scenario 9 (fault reset) pass.
- **Must not happen:** a web request delaying the control tick > 20 ms; secrets in responses or logs.
- **Done when:** `test_cycle_008*` passes and a manual check on two phones is recorded.

## Documentation Impact

- `README.md` — how to connect to the panel.
- `rodo/suntracker/project/architecture.md` — REST endpoint list if it changes.

## Open Questions

- none

## Decisions

- The HTTP layer is thin: `WebApi::handle(Request) -> Response` holds routing, login, validation and the mapping onto the control logic and is tested on the host; `lib/web/panel_server.cpp` only converts HTTP to that and serves the embedded gzip files. Because most of the risk is in the logic, and the network layer cannot run on the host.
- ESPAsyncWebServer (esphome fork) with one catch-all handler. A mutex shared with the control loop makes the API see state only between ticks; the control tick is therefore delayed by at most one handler (flash writes of settings or the log can take a few tens of ms). Accepted.
- Default passwords come from the untracked `firmware/src/secrets.h` (template: `secrets.example.h`); without it public development placeholders are used. The hotspot password and the panel password are separate, as the proposal says. Because the real shared default must not be committed.
- The hotspot password can be changed with `POST /api/settings {apPassword}`; it takes effect after the next restart and is never returned by any API or written to the event log.
- The panel has no server-side language switch: Persian, right-to-left, system fonts (no web font), numbers shown with Persian digits by the browser. Because the 150 KB limit and no internet for fonts.
- Unknown GET paths redirect to `/` so phones' captive-portal checks open the panel.
- One shared default password for all devices, changeable from the panel (no forced change). Because the user chose it (answer of 2026-10-29); the panel warns while the default is in use.
- Vanilla JS without a framework. Because the panel must be small enough to embed and work offline. (default, not asked)
- Polling every 1 s instead of WebSocket. Because it is simpler and robust with phones that sleep; traffic is tiny. (default, not asked)
