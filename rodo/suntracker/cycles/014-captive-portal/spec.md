# 014 — Captive portal: technical spec

## 1. Summary

Phones decide that a Wi-Fi network has a login page by asking a few well-known addresses. The hotspot answers every DNS name with its own address and answers those probes with a redirect to the panel, so the phone opens the panel by itself.

## 2. Rules

- **R1** A DNS server on UDP 53, bound to the hotspot interface only, answers every A query with 192.168.4.1 (TTL 0). It does not run on the site Wi-Fi interface.
- **R2** Probe URLs get a redirect (HTTP 302) to `http://192.168.4.1/` when the request arrived on the hotspot interface: `/generate_204`, `/gen_204` (Android), `/hotspot-detect.html`, `/library/test/success.html` (Apple), `/connecttest.txt`, `/ncsi.txt`, `/redirect` (Windows), `/canonical.html`, `/success.txt` (Firefox). Any other unknown GET from the hotspot side is redirected the same way.
- **R3** A request that arrived on the site Wi-Fi interface (any other local address) is redirected with a relative `Location: /` (so the LAN address keeps working, see cycle 008).
- **R4** The redirect decision is a pure function `portalRedirect(localIp, path)` returning "none", "absolute" or "relative".

## 3. Data

None.

## 4. API / logic

`web::portalRedirect(uint32_t localIpv4, const std::string& path)`; the server layer reads the local IP of the connection (`request->client()->localIP()`).

## 5. Screens

None. The phone shows its own sign-in notification.

## 6. Files to create or change

- `firmware/lib/core/webapi/portal.h` — the redirect decision.
- `firmware/lib/web/panel_server.cpp` — DNS server and redirect.
- `firmware/test/test_cycle_014_portal/test_main.cpp`

## 7. Edge cases

- A phone that has mobile data on keeps using it for the internet; the DNS answer only affects names resolved through the hotspot.
- iPhones show the sign-in page only if the probe answer is not the text "Success".

## Acceptance criteria

- **Must happen:** probes from the hotspot side redirect to the panel; LAN requests stay relative.
- **Must not happen:** the DNS server answering on the site Wi-Fi interface.
- **Done when:** host tests pass and a phone shows the sign-in notification.

## Documentation Impact

- `README.md` — the panel opens automatically.

## Open Questions

- none

## Decisions

- DNS hijack only on the hotspot. Because the site network must keep its own DNS.
