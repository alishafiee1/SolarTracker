# SunTracker — acceptance checklist (delivery day)

The 13 scenarios of the proposal (section 06), as run on the real device. Every scenario is run **three times**; write the date and a signature in the result columns. The same scenarios run on the simulator (`pio test -d firmware -e native -f "test_cycle_013*"`); scenarios 6, 7 and 10 need the optional modules 009 and 010 and are skipped there until those exist.

Before starting: flash the production image (`pio run -d firmware -e esp32dev -t upload`), make sure `firmware/src/secrets.h` holds the client's real default passwords, set the location, set the clock, and do the two-step angle calibration (panel horizontal, then a few degrees east).

Use the `esp32dev-bench` image only for the relay bench test below. It has a serial console that is **not** part of the delivered firmware.

| # | Scenario | Set-up on the device | Expected result | Run 1 (date, signature) | Run 2 | Run 3 |
|---|---|---|---|---|---|---|
| 1 | Jammed arm | Auto mode, panel 10+° from the target. Block the arm (or unplug the actuator) | Three tries 10 s apart, then power stays off; panel shows «خطا: گیرکردن بازو»; log has a Fault line | | | |
| 2 | Strong wind | Spin the anemometer above 40 km/h for more than 5 s | Panel goes to 0° (flat). Tracking returns only after 15 minutes below 30 km/h; a gust in between restarts the 15 minutes | | | |
| 3 | Night | Let the sun go below the horizon (or set the clock to night) | Panel goes to 0° and stays until sunrise | | | |
| 4 | Mains power cut | Cut the 220 V supply for a minute, restore | Clock keeps the time (CR2032), settings and calibration are unchanged, log shows `PowerUp` with the reset reason, relays are off at start-up | | | |
| 5 | Angle sensor fault | Unplug the MPU6050 while the panel moves | Motion stops within a second, panel shows «خطا: سنسور زاویه» | | | |
| 6 | Relay fault *(module 009)* | Keep current flowing after a stop command (bench set-up) | Current transformer sees it, CH1 is cut, warning on the panel | | | |
| 7 | End of stroke *(module 009)* | Let the actuator reach its internal limit | Power is cut, current reads zero, recognised as a limit, **no** fault | | | |
| 8 | Hotspot off | Switch the Wi-Fi off / leave the range | Tracking continues without interruption; only the panel is unreachable | | | |
| 9 | Fault reset | After scenario 1, free the arm and press «ریست خطا» | Fault cleared, control loop tries again and reaches the target | | | |
| 10 | Offline update *(module 010)* | Send a good `.bin` and a corrupted one from the phone | The good file installs; the corrupted one is refused and the old version stays | | | |
| 11 | Tracking accuracy | One full sunny day in Auto mode, log the angle | Panel is never more than 6° (threshold + 1°) from the sun target while the sun is up | | | |
| 12 | 72 hours unattended | Leave it running for 72 h | No unwanted reset, no false error, complete log | | | |
| 13 | Sudden reversal | Manual mode, moving east, press west | Power is cut first, then at least 3 s settle and a steady angle, then CH2 switches and the move starts; no jolt | | | |

## Relay bench test (before any actuator is connected)

Run with the actuators **disconnected**, relay inputs wired to LEDs or a meter, `esp32dev-bench` image:

```sh
python firmware/tools/bench_check_004.py COM7
```

Expect: no power in the first 3 s after boot, CH2 first and CH1 about 200 ms later, stop switches CH1 off at once, a reversal never switches CH2 under power and starts at least 10 s after the stop. Also look at the relay inputs with a scope or an LED during boot and reset: no pulse may appear (the software only reads pin levels after boot).

## One-day log check

After scenario 11, download the log from the panel (CSV when module 012 is installed) and check: no `Fault` lines, a `MoveStart`/`MoveEnd` pair per move, and the number of moves is plausible (about 20–30 per sunny day).
