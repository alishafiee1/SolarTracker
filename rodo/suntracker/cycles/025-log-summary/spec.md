# سند فنی: لاگ خلاصه

**R1.** `MoveStart` نوشته نمی‌شود. `MoveEnd` فقط وقتی نوشته می‌شود که علت توقف `stop` یا `reverse` نباشد یا حرکت از ۶۰ ثانیه بیشتر باشد.
**R2.** `MotionController` تعداد حرکت‌ها و مجموع زمان کار موتور را می‌شمارد (`moveCount`, `runMsTotal`).
**R3.** هر روز محلی یک خط `daily: moves= run_s= wind_max= bat_min= err_max=` برای روز تمام‌شده نوشته می‌شود؛ فقط وقتی ساعت معتبر است. با ریست دستگاه، شمارنده‌های همان روز از اول شروع می‌شوند (روز ناقص).
**R4.** رویدادهای تازه: `angle warning start/end`, `bale message failed`, `bale messages work again`, `mode auto/manual from the panel`.
**R5.** فیلتر «حرکت‌ها» از پنل برداشته شد؛ نوع‌های `MoveStart` و `MoveEnd` برای فایل‌های لاگ قدیمی و CSV می‌مانند.

## Decisions

- «حرکت غیرعادی» یعنی بیش از ۶۰ ثانیه یا علتی جز stop و reverse (default, not asked). Because حرکت عادی ۵ تا ۶ ثانیه است و علت‌های دیگر (timeout، interlock، قطع برای آپدیت) برای تشخیص خطا لازم‌اند.
