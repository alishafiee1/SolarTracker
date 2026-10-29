---
schemaVersion: 2
title: "پنل وب روی هات‌اسپات"
status: todo
priority: high
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/web/", "firmware/web/", "firmware/scripts/", "firmware/src/main.cpp", "firmware/test/test_cycle_008*/"]
  forbidden: ["firmware/lib/core/motion/", "hardware/", ".agents/"]
---

کاربر با گوشی به هات‌اسپات دستگاه وصل می‌شود و در مرورگر وضعیت، حرکت دستی، تنظیمات، کالیبراسیون، ساعت، موقعیت، لاگ و ریست خطا را دارد. حرکت و تنظیمات با رمز جداگانه محافظت می‌شوند.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/008-web-panel/simple.md)
- [سند فنی](/rodo/suntracker/cycles/008-web-panel/spec.md)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] هات‌اسپات با نام و رمز اختصاصی هر دستگاه (R1)
- [ ] وب‌سرور async و فایل‌های پنل فشرده داخل فرم‌ور، فارسی و راست‌به‌چپ (R2)
- [ ] API وضعیت و صفحه داشبورد (R3، R5)
- [ ] ورود با رمز تنظیمات، نشست و محدودیت تلاش (R4)
- [ ] صفحه‌های حرکت دستی، تنظیمات، موقعیت، ساعت، کالیبراسیون، لاگ و ریست خطا (R5، R6)
- [ ] وب هیچ‌وقت حلقه کنترل را معطل نمی‌کند (R7) و قطع WiFi روی کنترل اثری ندارد (R8)
- [ ] تست هندلرها روی native و بررسی دستی روی گوشی اندروید و آیفون
