# 007 — Persistent settings, clock and event log: technical spec

## 1. Summary

Versioned settings in NVS with validation, a UTC real-time clock with loss detection, a rotating event log on LittleFS, power-restored events and supply monitoring.

## 2. Rules

- **R1** `struct Settings` with a `version` field, compile-time defaults and a `validate()` that returns the list of invalid fields. Stored in NVS namespace `suntracker` as one blob plus CRC32. Saving rejects invalid values and keeps the old ones.
- **R2** On load: missing/CRC error → defaults + event `SettingsReset`; older version → migrate field by field (unknown fields get defaults) + event `SettingsMigrated`.
- **R3** `Ds3231Clock : IClock` keeps UTC. If the oscillator-stop flag is set (battery dead / first start), `clockValid` is false: status «ساعت تنظیم نشده» and no automatic tracking (005 R7) until the time is set.
- **R4** `setUtc(epoch)` sets the RTC and clears the flag; used by the web panel (phone time) and, if present, by NTP (011). Event `ClockSet old new`.
- **R5** Timezone offset stored in minutes (default +210 = +03:30); used only for display and CSV.
- **R6** `EventLog : IEventLog` on LittleFS: two files `/log/events.0.csv` and `/log/events.1.csv`, rotate when the active one exceeds 64 KB. Line: `utc_epoch,type,detail` (detail ≤ 80 chars, commas escaped). Writes are buffered and flushed at most every 10 s or on faults, to limit flash wear. Latest-N read API for the web panel.
- **R7** At boot an event `PowerUp reason` is written (power-on, watchdog, brownout, software, panic) — proposal scenario 4.
- **R8** `SupplyMonitor` reads GPIO 34 (divider) every second; below `supplyLowV` (default 4.5 V) for 3 s → event `SupplyLow` (no fault).
- **R9** Location: built-in list of Iranian province capitals (31 entries, id, Persian name, lat, lon), default Kerman (30.283, 57.083). Custom coordinates validated: lat −90…90, lon −180…180, parsed from "lat, lon" text as copied from Google Maps.

## 3. Data

Fields (defaults): latDeg 30.283, lonDeg 57.083, cityId kerman, tzOffsetMin 210, minDeg −45, maxDeg 45, moveThresholdDeg 5, stopSettleMs 3000, minMoveGapMs 10000, maxMoveMs 120000, stallCheckMs 5000, windStowKmh 40, windStowDelayS 5, windReturnKmh 30, windReturnMin 15, anemometerFactor 2.4, preDawnMinutes 0, mode Auto, angleCal (none), apPassword (set at production), settingsPasswordHash + salt, supplyLowV 4.5. Optional-module fields are added by their cycles.

## 4. API / logic

`ISettingsStore { bool load(Settings&); SaveResult save(const Settings&); }`, `IEventLog { void append(EventType, const char*); size_t latest(Event*, size_t n); }`.

## 5. Screens

None here; texts used by 008: «تنظیمات ذخیره شد», «مقدار نامعتبر: <field>», «ساعت تنظیم نشده».

## 6. Files to create or change

- `firmware/lib/core/settings/settings.{h,cpp}`, `firmware/lib/core/settings/cities_ir.h`
- `firmware/lib/core/events/event.h`
- `firmware/lib/drivers/nvs_settings_store.{h,cpp}`, `littlefs_event_log.{h,cpp}`, `ds3231_clock.{h,cpp}`, `supply_monitor.{h,cpp}`
- `firmware/test/test_cycle_007_settings/test_main.cpp`

## 7. Edge cases

- Power loss during a settings write → CRC catches it, previous blob kept (write new blob then switch key).
- Log flash full or LittleFS mount failure → format once, event `LogReset`; tracking continues without log.
- RTC drift: DS3231 ±2 ppm ≈ 1 min/year — fine for tracking (1 min ≈ 0.25°).

## Acceptance criteria

- **Must happen:** settings survive reboot; invalid values rejected (R1); RTC loss blocks auto tracking (R3); log rotates at 64 KB and keeps the newest lines (R6); proposal scenario 4 (power loss) produces a `PowerUp` event and keeps time.
- **Must not happen:** a corrupted blob crashing the boot; unbounded log growth.
- **Done when:** `test_cycle_007*` passes on native; a power-cut test on hardware is recorded.

## Documentation Impact

- `rodo/suntracker/project/architecture.md` — `Settings` field list if names or defaults change.

## Open Questions

- none

## Decisions

- Two 64 KB rotating files. Because the 512 KB LittleFS partition must also hold nothing else, and ~1500 events cover weeks of normal operation. (default, not asked)
- Province-capital list only. Because coordinates can always be pasted for any other place. (default, not asked)
