"""Hardware check for cycle 001 on a real ESP32 flashed with env esp32dev-bench.

Usage: python firmware/tools/bench_check_001.py COM7
Checks: boot banner (R7), relay pins inactive after boot (R4, level only),
control tick rate (R6), watchdog reset after a hang (R5).
Boot-time glitch-free ordering of R4 still needs a scope or LED on the relay inputs.
"""
import re
import sys
import time

import serial

results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (" — " + detail if detail else ""))


def reset(ser):
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False


def read_until(ser, pattern, timeout):
    end = time.time() + timeout
    seen = []
    while time.time() < end:
        line = ser.readline().decode("utf-8", "replace").rstrip()
        if line:
            seen.append(line)
            if re.search(pattern, line):
                return line, seen
    return None, seen


def ask(ser, cmd, pattern, timeout=2.0):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    line, _ = read_until(ser, pattern, timeout)
    return line


def main():
    port = sys.argv[1]
    with serial.Serial(port, 115200, timeout=0.2) as ser:
        reset(ser)
        banner, _ = read_until(ser, r"SunTracker .* reset: ", 5)
        check("R7 boot banner", banner is not None, banner or "no banner")
        check("R5 reset reason after reset pin is power-on", bool(banner and "reset: power-on" in banner))

        pins = ask(ser, "pins", r"pins:")
        check("R4 relay pins inactive after boot", bool(pins and "CH1(power)=1" in pins and "CH2(direction)=1" in pins), pins or "")

        s1 = ask(ser, "status", r"status:")
        time.sleep(2.0)
        s2 = ask(ser, "status", r"status:")
        if s1 and s2:
            m1 = re.search(r"uptime_ms=(\d+) ticks=(\d+)", s1)
            m2 = re.search(r"uptime_ms=(\d+) ticks=(\d+)", s2)
            du = int(m2.group(1)) - int(m1.group(1))
            dt = int(m2.group(2)) - int(m1.group(2))
            rate = dt * 1000.0 / du if du else 0
            check("R6 control tick about 10 Hz", 9.0 <= rate <= 11.0, f"{rate:.2f} ticks/s over {du} ms")
        else:
            check("R6 control tick about 10 Hz", False, "no status reply")

        ser.reset_input_buffer()
        ser.write(b"hang\n")
        t0 = time.time()
        line, seen = read_until(ser, r"SunTracker .* reset: ", 12)
        dt = time.time() - t0
        ok = bool(line and "reset: watchdog" in line and 4.0 <= dt <= 8.0)
        check("R5 hang triggers a watchdog reset in about 5 s", ok, f"{line} after {dt:.1f} s" if line else "no reset seen")
        pins = ask(ser, "pins", r"pins:")
        check("R4 relay pins inactive after watchdog reset", bool(pins and "CH1(power)=1" in pins and "CH2(direction)=1" in pins), pins or "")

    print("ALL PASS" if all(results) else "SOME FAILED")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
