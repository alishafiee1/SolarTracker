---
schemaVersion: 2
title: "خروجی CSV لاگ (اختیاری)"
status: todo
priority: low
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/web/", "firmware/web/", "firmware/lib/drivers/littlefs*", "firmware/test/test_cycle_012*/"]
  forbidden: ["firmware/lib/core/", "hardware/", ".agents/"]
---

ماژول اختیاری ج۶. لاگ رویدادهای دستگاه از پنل وب به‌صورت یک فایل CSV دانلود می‌شود که در اکسل درست باز می‌شود.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/012-log-csv-export/simple.md)
- [سند فنی](/rodo/suntracker/cycles/012-log-csv-export/spec.md)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)

## Checklist: قدم‌ها

- [ ] دکمه دانلود در صفحه لاگ (R1)
- [ ] تولید جریانی CSV از قدیمی به جدید با زمان محلی و UTC (R2، R3)
- [ ] باز شدن درست در اکسل با حروف فارسی (R4)
