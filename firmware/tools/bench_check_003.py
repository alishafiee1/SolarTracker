"""Hardware check for cycle 003 on a real ESP32 flashed with env esp32dev-bench.

Usage: python firmware/tools/bench_check_003.py COM7 [--sensor]
Without --sensor (no MPU6050 wired): the board must latch a sensor fault within 1 s of boot (R5)
and refuse automatic use (R6). With --sensor (MPU6050 on SDA 21 / SCL 22, AD0 to 3.3 V): WHO_AM_I
must be 0x68 and |g| about 1 g with the board at rest.
The tilt/calibration steps need a person moving the board; they are done with the console
commands `cal h`, `cal e`, `angle` (see the bench_console.h header).
"""
import re
import sys
import time

import serial

results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (" — " + detail if detail else ""))


def ask(ser, cmd, pattern, timeout=2.0):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    end = time.time() + timeout
    while time.time() < end:
        line = ser.readline().decode("utf-8", "replace").rstrip()
        if re.search(pattern, line):
            return line
    return None


def main():
    port = sys.argv[1]
    sensor = "--sensor" in sys.argv
    with serial.Serial(port, 115200, timeout=0.2) as ser:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        time.sleep(3.0)  # boot + a second of sampling

        line = ask(ser, "angle", r"I angle:")
        check("angle command answers", line is not None, line or "")
        if line is None:
            sys.exit(1)
        m = re.search(r"whoami=0x([0-9A-F]{2}) read=(\d) .*\|g\|=([\d.]+) calibrated=(\d) .* ok=(\d)", line)
        who, read, mag, cal, ok = int(m.group(1), 16), int(m.group(2)), float(m.group(3)), int(m.group(4)), int(m.group(5))

        check("R6 not calibrated after boot", cal == 0)
        if sensor:
            check("R1 WHO_AM_I is 0x68", who == 0x68, "0x%02X" % who)
            check("sensor read works", read == 1)
            check("gravity about 1 g at rest", 0.9 <= mag <= 1.1, "|g| = %.3f" % mag)
        else:
            check("R5 missing sensor latches a fault within the first seconds", ok == 0, "ok=%d whoami=0x%02X" % (ok, who))
            check("missing sensor reports no data", read == 0 and who == 0)

        line = ask(ser, "cal e", r"cal:")
        check("east capture is refused before the horizontal one", line is not None and line.startswith("E "), line or "")

        line = ask(ser, "status", r"I status:")
        check("control loop still runs next to the sensor retries", line is not None and "ticks=" in line, line or "")

    print("ALL PASS" if all(results) else "SOME FAILED")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
