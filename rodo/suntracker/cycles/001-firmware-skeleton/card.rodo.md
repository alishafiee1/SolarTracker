---
schemaVersion: 2
title: "اسکلت فرم‌ور و لایه سخت‌افزار"
status: todo
priority: high
workflow: major
members: []
scope:
  allowed: ["firmware/platformio.ini", "firmware/partitions.csv", "firmware/src/", "firmware/lib/core/", "firmware/lib/hal/", "firmware/lib/sim/", "firmware/test/test_cycle_001*/", "README.md"]
  forbidden: ["hardware/", "enclosure/", "docs/", ".agents/", "rodo/kit.json"]
---

پروژه فرم‌ور ESP32 ساخته می‌شود که با روشن شدن، اول از همه برق جک را قطع نگه می‌دارد، watchdog دارد و تست‌هایش روی کامپیوتر اجرا می‌شوند. همه چرخه‌های بعدی روی این اسکلت ساخته می‌شوند.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/001-firmware-skeleton/simple.md)
- [سند فنی](/rodo/suntracker/cycles/001-firmware-skeleton/spec.md)
- [طراحی](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [سناریوهای پذیرش](/rodo/suntracker/cycles/001-firmware-skeleton/acceptance.feature)
- [معماری](/rodo/suntracker/project/architecture.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] پروژه PlatformIO با دو محیط esp32dev و native ساخته شد و هر دو بیلد می‌شوند (R1)
- [ ] پوشه‌های lib/core، lib/hal، lib/drivers، lib/sim، lib/web و test طبق R2 ساخته شدند
- [ ] رابط‌های HAL و پیاده‌سازی جعلی آنها در lib/sim نوشته شد (R3)
- [ ] ترتیب راه‌اندازی امن: رله‌ها قبل از هر کار دیگری خاموش (R4)
- [ ] watchdog پنج ثانیه‌ای و ثبت دلیل ریست (R5)
- [ ] حلقه کنترل ۱۰۰ میلی‌ثانیه‌ای با ControlLoop::tick خالی (R6)
- [ ] نسخه فرم‌ور در بوت چاپ می‌شود و جدول پارتیشن دو اسلات OTA دارد (R7، R8)
- [ ] تست‌های test_cycle_001 روی native پاس شدند و README بخش ساخت و تست را دارد
- [ ] تست پذیرش پاس شد
