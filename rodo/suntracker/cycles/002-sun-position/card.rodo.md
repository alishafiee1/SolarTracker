---
schemaVersion: 2
title: "محاسبه موقعیت خورشید و زاویه هدف"
status: todo
priority: high
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/core/sun/", "firmware/test/test_cycle_002*/", "firmware/test/fixtures/", "firmware/tools/"]
  forbidden: ["firmware/lib/drivers/", "firmware/lib/web/", "hardware/", ".agents/"]
---

دستگاه از روی ساعت و مختصات محل، جای خورشید و زاویه‌ای را که پنل باید داشته باشد حساب می‌کند. این عدد پایه ردیابی خودکار است.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/002-sun-position/simple.md)
- [سند فنی](/rodo/suntracker/cycles/002-sun-position/spec.md)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] الگوریتم PSA+ در lib/core/sun پیاده شد (R1)
- [ ] جدول مرجع NREL SPA برای کرمان ساخته و در fixtures ذخیره شد (R2)
- [ ] تست دقت در برابر جدول مرجع پاس شد: خطا ≤ ۰٫۰۵ درجه (R2)
- [ ] فرمول زاویه چرخش و محدودسازی به حداقل/حداکثر نوشته و تست شد (R3، R4)
- [ ] تشخیص شب از روی ارتفاع خورشید (R5)
- [ ] زمان اجرا روی ESP32 یک بار اندازه‌گیری و در Progress notes ثبت شد (R6)
