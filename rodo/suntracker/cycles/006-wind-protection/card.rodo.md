---
schemaVersion: 2
title: "حفاظت باد شدید"
status: todo
priority: high
workflow: standard
members: []
scope:
  allowed: ["firmware/lib/core/wind/", "firmware/lib/drivers/anemometer*", "firmware/lib/sim/", "firmware/test/test_cycle_006*/"]
  forbidden: ["firmware/lib/core/motion/", "firmware/lib/web/", "hardware/", ".agents/"]
---

بادسنج خوانده می‌شود و اگر باد بیش از ۵ ثانیه از ۴۰ کیلومتر بر ساعت بیشتر باشد، پنل افقی می‌شود. وقتی باد ۱۵ دقیقه پیوسته زیر ۳۰ بماند، ردیابی برمی‌گردد.

## Progress notes

- Updated: 2026-10-29
- Done: هنوز شروع نشده
- Remaining: همهٔ قدم‌ها

## Refs

- [توضیح ساده](/rodo/suntracker/cycles/006-wind-protection/simple.md)
- [سند فنی](/rodo/suntracker/cycles/006-wind-protection/spec.md)
- [سناریوهای پذیرش](/rodo/suntracker/cycles/006-wind-protection/acceptance.feature)
- [معماری](/rodo/suntracker/project/architecture.md)
- [طراحی کلی فرم‌ور (چرخه 001)](/rodo/suntracker/cycles/001-firmware-skeleton/design.md)
- [پروپوزال v1.8](/docs/solar_tracker_proposal.html)

## Checklist: قدم‌ها

- [ ] شمارش پالس با وقفه روی GPIO 4 و حذف نویز (R1)
- [ ] تبدیل پالس به سرعت با ضریب قابل تنظیم و میانگین ۳ ثانیه‌ای (R2)
- [ ] ورود به حالت باد بعد از ۵ ثانیه بالای ۴۰ (R3)
- [ ] خروج بعد از ۱۵ دقیقه پیوسته زیر ۳۰ و ریست تایمر با تندباد (R4)
- [ ] اعتبارسنجی آستانه‌ها (R6)
- [ ] سناریوهای acceptance.feature در شبیه‌ساز پاس شدند
- [ ] تست پذیرش پاس شد
