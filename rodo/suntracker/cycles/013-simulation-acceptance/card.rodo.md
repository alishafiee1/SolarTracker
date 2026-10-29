---
schemaVersion: 2
title: "شبیه‌ساز و سناریوهای پذیرش"
status: todo
priority: high
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/sim/", "firmware/test/test_cycle_013*/", "docs/acceptance-checklist.md"]
  forbidden: ["firmware/lib/core/", "firmware/lib/drivers/", "hardware/", ".agents/"]
---

مدل نرم‌افزاری جک، باد و سنسورها ساخته می‌شود تا ۱۳ سناریوی پذیرش پروپوزال قبل از نصب روی کامپیوتر اجرا شوند. یک چک‌لیست تست میدانی هم برای روز تحویل آماده می‌شود.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/013-simulation-acceptance/simple.md)
- [سند فنی](/rodo/suntracker/cycles/013-simulation-acceptance/spec.md)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [طراحی حرکت ایمن (چرخه 004)](/rodo/suntracker/cycles/004-safe-motion/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] مدل جک (سرعت، توقف تدریجی، لیمیت، گیرکردن)، مدل باد و نویز سنسورها (R1)
- [ ] اجرای سناریوهای ۱ تا ۱۳ با سه seed مختلف (R2)
- [ ] شبیه‌سازی روز کامل در ۴ تاریخ با خطای کمتر از ۶ درجه (R3)
- [ ] شبیه‌سازی ۷۲ ساعته بدون خطای کاذب و با لاگ محدود (R4)
- [ ] چک‌لیست تست میدانی در docs/acceptance-checklist.md (R5)
