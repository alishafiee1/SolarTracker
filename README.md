# SunTracker — Single-Axis Solar Tracker Controller

کنترلر تراکر خورشیدی تک‌محوره با ESP32: زاویهٔ پنل را بر اساس موقعیت لحظه‌ای خورشید تنظیم می‌کند، در باد شدید پنل را به حالت ایمن می‌برد، گیرکردن بازو را تشخیص می‌دهد و از طریق پنل وب روی هات‌اسپات خود دستگاه کنترل می‌شود.

An ESP32-based controller for a single-axis solar tracker. It computes the sun position, drives a 220 V actuator through relays with a hardware direction interlock (stop / east / west), stows the panel in high wind, detects a stuck arm, and serves a local web panel over its own Wi-Fi hotspot.

## Status

Planning. The technical proposal is in [`docs/solar_tracker_proposal.html`](docs/solar_tracker_proposal.html). No firmware or hardware files yet.

## Scope

- In scope: controller board, sensors (tilt, anemometer, current), firmware, web panel.
- Out of scope: the mechanical structure and the actuator itself.

## Repository layout

| Path | Contents |
|---|---|
| `docs/` | Proposal and project documentation |
| `rodo/` | Planning with the RODO method (brief, architecture, map, memory) |
| `.agents/skills/rodo/` | RODO kit files — do not edit; checksums in `rodo/kit.json` |
| `firmware/` | *(planned)* PlatformIO project for the ESP32 |
| `hardware/` | *(planned)* Schematic, PCB (KiCad) and BOM |
| `enclosure/` | *(planned)* Enclosure models (STEP / STL) |

## Working with RODO

This repo is planned with RODO. Agents and contributors start with [`AGENTS.md`](AGENTS.md) and [`rodo/suntracker/MAP.md`](rodo/suntracker/MAP.md).

## License

Copyright (c) 2026 Ali Shafiee. All rights reserved. See [`LICENSE`](LICENSE).
