---
schemaVersion: 2
title: "خواندن و کالیبره زاویه پنل"
status: todo
priority: high
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/core/angle/", "firmware/lib/drivers/mpu6050*", "firmware/lib/sim/", "firmware/test/test_cycle_003*/"]
  forbidden: ["firmware/lib/web/", "hardware/", ".agents/"]
---

دستگاه زاویه واقعی پنل را از شتاب‌سنج روی برد می‌خواند. جعبه با هر جهتی نصب شود، با کالیبراسیون دو مرحله‌ای زاویه درست و جهت شرق و غرب معلوم می‌شود. اگر سنسور خراب شود، حرکت متوقف می‌شود.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/003-panel-angle/simple.md)
- [سند فنی](/rodo/suntracker/cycles/003-panel-angle/spec.md)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] درایور MPU6050 روی آدرس 0x69 با بررسی WHO_AM_I (R1)
- [ ] ریاضی کالیبراسیون دو مرحله‌ای و محاسبه زاویه علامت‌دار (R2)
- [ ] رد کالیبراسیون نامعتبر با پیام روشن (R3)
- [ ] میانگین متحرک ۵ نمونه‌ای با نمونه‌برداری ۱۰ هرتز (R4)
- [ ] تشخیص خطای سنسور در کمتر از ۱ ثانیه (R5)
- [ ] بدون کالیبراسیون، حرکت خودکار مجاز نیست (R6)
- [ ] تست با بردارهای گرانش مصنوعی در جهت‌های نصب مختلف پاس شد
