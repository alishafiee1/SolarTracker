---
schemaVersion: 2
title: "ماشین حالت کنترل و تشخیص گیرکردن"
status: todo
priority: high
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/core/control/", "firmware/lib/core/control_loop*", "firmware/lib/sim/", "firmware/test/test_cycle_005*/"]
  forbidden: ["firmware/lib/core/motion/", "firmware/lib/web/", "hardware/", ".agents/"]
---

مغز دستگاه ساخته می‌شود: در هر لحظه تصمیم می‌گیرد پنل در حالت خطا، باد، دستی، شب یا اتوماتیک باشد و کجا برود. اگر بازو گیر کند، سه بار تلاش می‌کند و بعد می‌ایستد و خطا نشان می‌دهد.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/005-control-state-machine/simple.md)
- [سند فنی](/rodo/suntracker/cycles/005-control-state-machine/spec.md)
- [سناریوهای پذیرش](/rodo/suntracker/cycles/005-control-state-machine/acceptance.feature)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [طراحی حرکت ایمن (چرخه 004)](/rodo/suntracker/cycles/004-safe-motion/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] حالت‌ها با اولویت خطا > باد > دستی > شب > اتو پیاده شدند (R1)
- [ ] ردیابی اتوماتیک با حد آستانه ۵ درجه و توقف در ±۱ درجه (R2)
- [ ] حالت شب و آماده‌سازی اختیاری قبل از طلوع (R3)
- [ ] فرمان‌های حالت دستی و ماندن در دستی (R4)
- [ ] تشخیص گیرکردن با زاویه، ۳ تلاش با فاصله ۱۰ ثانیه (R5)
- [ ] قفل خطا، استثنای باد و ریست خطا (R6، R7، R8)
- [ ] ثبت رویداد برای هر تغییر حالت و خطا (R9)
- [ ] سناریوهای acceptance.feature در شبیه‌ساز پاس شدند
- [ ] تست پذیرش پاس شد
