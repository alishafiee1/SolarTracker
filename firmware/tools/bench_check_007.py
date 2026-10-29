"""Hardware check for cycle 007 on a real ESP32 flashed with env esp32dev-bench.

Usage: python firmware/tools/bench_check_007.py COM7
Checks: settings survive a reboot (R1/R2), invalid values are refused (R1), the PowerUp event with the
reset reason is stored in flash (R7), the log rotates at 64 KB and keeps the newest lines (R6), and the
missing DS3231 is reported as lost power (R3). It changes the stored move threshold and resets it at the end.
The log lives in flash and keeps its content between runs, so the rotation check is relative.
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
    time.sleep(3.5)  # boot, mount, load settings
    ser.reset_input_buffer()


def ask(ser, cmd, until=r"^I |^E ", timeout=4.0, collect=False):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    end = time.time() + timeout
    lines = []
    while time.time() < end:
        line = ser.readline().decode("utf-8", "replace").rstrip()
        if not line or "Wire.cpp" in line:  # I2C error noise from the missing DS3231
            continue
        lines.append(line)
        if not collect and re.search(until, line):
            return line
    return "\n".join(lines) if collect else None


def log_size(ser):
    line = ask(ser, "log 1", r"I log:") or ""
    m = re.search(r"active file (\d) \((\d+) bytes\)", line)
    return (int(m.group(1)), int(m.group(2))) if m else (None, None)


def main():
    with serial.Serial(sys.argv[1], 115200, timeout=0.3) as ser:
        reset(ser)
        ask(ser, "set reset", r"set:")
        line = ask(ser, "settings", r"I settings:")
        check("defaults after reset", line is not None and "thr=5.0" in line and "city=kerman" in line and "valid=1" in line, line or "")

        line = ask(ser, "set thr 7.5", r"set:")
        check("R1 a valid change is saved", line is not None and "saved thr=7.5" in line, line or "")
        line = ask(ser, "set thr 99", r"set:")
        check("R1 an invalid value is refused", line is not None and "rejected" in line and "moveThresholdDeg" in line, line or "")
        line = ask(ser, "settings", r"I settings:")
        check("R1 the refused value did not change anything", line is not None and "thr=7.5" in line, line or "")

        ser.write(b"reboot\n")
        time.sleep(4.5)
        ser.reset_input_buffer()
        line = ask(ser, "settings", r"I settings:")
        check("R2 settings survive a reboot", line is not None and "thr=7.5" in line, line or "")

        out = ask(ser, "log 20", collect=True, timeout=3.0)
        check("R7 PowerUp event with the reset reason is stored", "PowerUp,software" in out, out.splitlines()[-1] if out else "")

        reset(ser)
        out = ask(ser, "log 20", collect=True, timeout=3.0)
        check("R7 power-on reset is recorded", "PowerUp,power-on" in out)

        line = ask(ser, "clock", r"I clock:")
        check("R3 missing DS3231 is reported as lost power", line is not None and "present=0" in line and "lost_power=1" in line, line or "")

        idx0, size0 = log_size(ser)
        line = ask(ser, "loggen 1500", r"I loggen:", timeout=60)
        m = re.search(r"active file (\d) is (\d+) bytes", line or "")
        idx1, size1 = (int(m.group(1)), int(m.group(2))) if m else (None, None)
        written = 1500 * 53  # about 53 bytes per generated line
        check("R6 the active file stays below 64 KB", size1 is not None and size1 < 64 * 1024, line or "")
        check("R6 the log rotated (more than 64 KB were written in total)", size1 is not None and size0 is not None and (idx1 != idx0 or size1 < size0 + written - 30000), "before %s/%s, after %s/%s" % (idx0, size0, idx1, size1))

        out = ask(ser, "log 3", collect=True, timeout=4.0)
        check("R6 the newest lines are returned", "test event 1499" in out, out.replace("\n", " | ")[-100:])
        out = ask(ser, "log 30", collect=True, timeout=6.0)  # the bench console prints at most 1800 bytes
        check("R6 30 lines can be read back across the rotation", out.count("test event") >= 28, "%d lines" % out.count("test event"))

        ask(ser, "set reset", r"set:")
    print("ALL PASS" if all(results) else "SOME FAILED")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
