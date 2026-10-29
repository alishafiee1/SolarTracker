"""Hardware check for cycle 006 on a real ESP32 flashed with env esp32dev-bench.

Usage: python firmware/tools/bench_check_006.py COM7
The bench console can make the real anemometer pin (GPIO 4) produce falling edges (`windgen <hz>`), so
the interrupt, the pulse ring buffer, the speed calculation and the stow logic run on the real chip.
Checks: calm without pulses, speed from the pulse rate (default factor 6.8 km/h per Hz), stow after more
than 5 s above 40 km/h and the stow event in the log. The 15 minute return is covered by the host tests.
"""
import re
import sys
import time

import serial

results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (" — " + detail if detail else ""))


def ask(ser, cmd, pattern, timeout=3.0):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    end = time.time() + timeout
    while time.time() < end:
        line = ser.readline().decode("utf-8", "replace").rstrip()
        if "Wire.cpp" in line:
            continue
        if re.search(pattern, line):
            return line
    return None


def wind(ser):
    line = ask(ser, "wind", r"I wind:")
    m = re.search(r"speed=([\d.]+) stow=(\d) return_in_s=(\d+) pulses=(\d+)", line or "")
    return (float(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))) if m else None, line


def main():
    with serial.Serial(sys.argv[1], 115200, timeout=0.3) as ser:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        time.sleep(3.5)
        ask(ser, "set reset", r"set:")

        w, line = wind(ser)
        check("calm: no pulses, speed 0, no stow", w is not None and w[0] == 0.0 and w[1] == 0 and w[3] == 0, line or "")

        ask(ser, "windgen 2", r"windgen")
        time.sleep(7.0)
        w, line = wind(ser)
        check("2 Hz gives about 13.6 km/h", w is not None and 12.5 <= w[0] <= 14.7 and w[1] == 0, line or "")

        ask(ser, "windgen 7", r"windgen")  # 7 Hz x 6.8 = 47.6 km/h
        time.sleep(3.0)
        w3, line3 = wind(ser)
        check("47.6 km/h after 3 s: not stowed yet", w3 is not None and 44 <= w3[0] <= 51 and w3[1] == 0, line3 or "")
        time.sleep(5.0)
        w8, line8 = wind(ser)
        check("stow is on after more than 5 s above 40 km/h", w8 is not None and w8[1] == 1 and w8[2] > 800, line8 or "")

        ask(ser, "windgen 0", r"windgen")
        time.sleep(4.0)
        w, line = wind(ser)
        check("pulses stop: speed back to 0, stow stays on (15 min wait)", w is not None and w[0] == 0.0 and w[1] == 1 and w[2] > 800, line or "")

        ser.write(b"log 6\n")
        time.sleep(2.0)
        text = ser.read(4000).decode("utf-8", "replace")
        check("stow start is in the log", "wind stow start" in text, "")

        ser.write(b"reboot\n")  # the stow state is not persistent: a reboot starts calm
        time.sleep(4.5)
        w, line = wind(ser)
        check("after a reboot the stow flag is cleared", w is not None and w[1] == 0, line or "")
    print("ALL PASS" if all(results) else "SOME FAILED")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
