---
schemaVersion: 2
title: "حفاظت جریان"
status: todo
priority: medium
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/core/current/", "firmware/lib/drivers/sct013*", "firmware/lib/core/control/", "firmware/lib/sim/", "firmware/test/test_cycle_009*/"]
  forbidden: ["firmware/lib/core/motion/motion_controller.cpp", "hardware/", ".agents/"]
---

ماژول ب۲ + ج۲ (کارفرما همه ماژول‌ها را خواسته است). با ترانس جریان، گیرکردن (جریان بالا)، رسیدن به لیمیت (جریان صفر) و چسبیدن رله (جریان بعد از توقف) از هم تشخیص داده می‌شوند.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/009-current-protection/simple.md)
- [سند فنی](/rodo/suntracker/cycles/009-current-protection/spec.md)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [طراحی حرکت ایمن (چرخه 004)](/rodo/suntracker/cycles/004-safe-motion/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] نمونه‌برداری ADC و محاسبه RMS با ضریب کالیبراسیون (R1)
- [ ] تشخیص گیرکردن با جریان بالا (R2)
- [ ] تشخیص لیمیت با جریان صفر و تفکیک از قطع سیم (R3)
- [ ] تشخیص چسبیدن رله و اضافه جریان (R4، R5)
- [ ] اتصال به ماشین حالت و شرط تعویض جهت در 004
- [ ] سناریوهای ۶ و ۷ پروپوزال در شبیه‌ساز پاس شدند
