---
schemaVersion: 2
title: "تنظیمات ماندگار، ساعت و لاگ رویداد"
status: todo
priority: high
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/core/settings/", "firmware/lib/core/events/", "firmware/lib/drivers/nvs*", "firmware/lib/drivers/littlefs*", "firmware/lib/drivers/ds3231*", "firmware/lib/drivers/supply*", "firmware/lib/sim/", "firmware/test/test_cycle_007*/"]
  forbidden: ["firmware/lib/web/", "firmware/lib/core/motion/", "hardware/", ".agents/"]
---

تنظیمات دستگاه بعد از قطع برق می‌ماند، ساعت به وقت UTC نگه داشته می‌شود و هر خطا، حرکت و تغییر حالت با زمان روی حافظه دستگاه ثبت می‌شود. قطع برق شبکه بعد از برگشت برق ثبت می‌شود.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/007-settings-clock-log/simple.md)
- [سند فنی](/rodo/suntracker/cycles/007-settings-clock-log/spec.md)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] ساختار Settings با نسخه، پیش‌فرض‌ها و بازه مجاز هر فیلد (R1)
- [ ] ذخیره و بازیابی در NVS و مهاجرت نسخه (R1، R2)
- [ ] درایور DS3231 با UTC و تشخیص از دست رفتن ساعت (R3)
- [ ] تنظیم ساعت و منطقه زمانی (R4، R5)
- [ ] لاگ چرخشی روی LittleFS (R6)
- [ ] ثبت رویداد برگشت برق با دلیل ریست و هشدار ولتاژ پایین (R7، R8)
- [ ] فهرست شهرهای ایران و اعتبارسنجی مختصات (R9)
