---
schemaVersion: 2
title: "حرکت ایمن جک با قفل جهت"
status: todo
priority: high
workflow: major
members: []
scope:
  allowed: ["firmware/lib/core/motion/", "firmware/lib/drivers/relays_gpio*", "firmware/lib/sim/", "firmware/test/test_cycle_004*/"]
  forbidden: ["firmware/lib/web/", "hardware/", ".agents/"]
---

تنها بخشی از نرم‌افزار که رله‌ها را روشن و خاموش می‌کند ساخته می‌شود. ترتیب «اول جهت، بعد برق»، مکث بعد از توقف، فاصله بین دو حرکت و سقف زمان حرکت را در همه حالت‌ها رعایت می‌کند.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/004-safe-motion/simple.md)
- [سند فنی](/rodo/suntracker/cycles/004-safe-motion/spec.md)
- [طراحی](/rodo/suntracker/cycles/004-safe-motion/design.md)
- [سناریوهای پذیرش](/rodo/suntracker/cycles/004-safe-motion/acceptance.feature)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] MotionController با حالت‌های Idle، Selecting، Running، Settling (R1)
- [ ] ترتیب حرکت: جهت، ۲۰۰ میلی‌ثانیه، برق (R2)
- [ ] توقف فوری و مکث ۳ ثانیه‌ای بعد از آن (R3)
- [ ] شرط تعویض جهت: مکث + جریان صفر + زاویه ثابت (R4)
- [ ] فاصله ۱۰ ثانیه‌ای بین دو حرکت و سقف ۱۲۰ ثانیه حرکت (R5، R6)
- [ ] تعویض کانال ۲ زیر بار با ساختار کد غیرممکن است (R8)
- [ ] قطع اضطراری بدون تأخیر (R10)
- [ ] همه سناریوهای acceptance.feature در شبیه‌ساز پاس شدند
- [ ] تست پذیرش پاس شد
