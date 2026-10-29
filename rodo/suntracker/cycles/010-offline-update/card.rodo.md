---
schemaVersion: 2
title: "به‌روزرسانی آفلاین از وب (اختیاری)"
status: todo
priority: low
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/web/", "firmware/lib/drivers/ota*", "firmware/web/", "firmware/src/main.cpp", "firmware/test/test_cycle_010*/"]
  forbidden: ["firmware/lib/core/motion/", "firmware/partitions.csv", "hardware/", ".agents/"]
---

ماژول اختیاری ج۴. فایل نسخه جدید از گوشی و روی هات‌اسپات نصب می‌شود. فایل خراب رد می‌شود و اگر نسخه جدید درست کار نکند، دستگاه خودش به نسخه قبلی برمی‌گردد.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/010-offline-update/simple.md)
- [سند فنی](/rodo/suntracker/cycles/010-offline-update/spec.md)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] صفحه به‌روزرسانی و آپلود فایل .bin با رمز (R1)
- [ ] قطع برق جک و توقف کنترل در حین به‌روزرسانی (R2)
- [ ] نوشتن در اسلات غیرفعال و بررسی سلامت فایل (R3)
- [ ] تأیید نسخه جدید بعد از خودآزمایی و بازگشت خودکار (R4)
- [ ] سناریوی ۱۰ پروپوزال روی برد واقعی سه بار انجام شد
