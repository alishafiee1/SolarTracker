---
schemaVersion: 2
title: "WiFi محل و اعلان بله"
status: todo
priority: medium
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/net/", "firmware/lib/web/", "firmware/web/", "firmware/lib/core/settings/", "firmware/test/test_cycle_011*/"]
  forbidden: ["firmware/lib/core/motion/", "hardware/", ".agents/"]
---

ماژول ج۵ (کارفرما همه ماژول‌ها را خواسته است). دستگاه علاوه بر هات‌اسپات به WiFi محل وصل می‌شود، ساعت را از اینترنت همگام می‌کند و خطاها را به ربات بله می‌فرستد. قطع WiFi روی کار تراکر اثری ندارد.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/011-site-wifi-bale/simple.md)
- [سند فنی](/rodo/suntracker/cycles/011-site-wifi-bale/spec.md)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] حالت هم‌زمان هات‌اسپات و اتصال به WiFi محل با تلاش دوباره (R1)
- [ ] همگام‌سازی ساعت با NTP (R2)
- [ ] ارسال خطا به ربات بله با صف و محدودیت (R3)
- [ ] تنظیمات WiFi و ربات در پنل وب، بدون نمایش رمزها (R4)
- [ ] بررسی کردیم پنل روی اینترنت باز نیست (R5)
