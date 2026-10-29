"""Hardware check for cycle 010 (probation and rollback) on a real ESP32.

Preparation (see the checklist at the end of docs/acceptance-checklist.md):
  1. build an image with SUNTRACKER_VERSION=OLD-rollback-target and write it to the second OTA slot:
       esptool.py --chip esp32 --port COM7 write_flash 0x190000 old_image.bin
  2. build another image with SUNTRACKER_VERSION=NEW-under-test and upload it normally (it lands in the first slot).
Usage: python firmware/tools/bench_check_010.py COM7
Checks: the new image on probation counts its boots, the fourth start switches to the old image, the old image runs
normally afterwards, and an image that stays up for 60 s with a working control loop confirms itself.
The upload over HTTP needs a phone on the hotspot and is checked by hand.
"""
import re
import sys
import time

import serial

results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (" — " + detail if detail else ""))


def boot(ser, wait=5.5):
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    time.sleep(wait)
    text = ser.read(4000).decode("utf-8", "replace")
    found = re.findall(r"I SunTracker (\S+) ", text)
    return found[-1] if found else None  # after a rollback two banners follow each other: the last one counts


def ask(ser, cmd, pattern, timeout=3.0):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    end = time.time() + timeout
    while time.time() < end:
        line = ser.readline().decode("utf-8", "replace").rstrip()
        if re.search(pattern, line):
            return line
    return None


def main():
    with serial.Serial(sys.argv[1], 115200, timeout=0.3) as ser:
        v = boot(ser)
        check("the new image is running", v == "NEW-under-test", str(v))

        line = ask(ser, "ota arm", r"I ota:")
        check("probation armed", line is not None and "probation=1 boots_on_probation=0" in line, line or "")
        tries = []
        for i in range(3):
            v = boot(ser)
            line = ask(ser, "ota", r"I ota:")
            tries.append((v, line))
        check("boots 1 to 3 on probation stay on the new image and are counted",
              all(t[0] == "NEW-under-test" and t[1] and ("boots_on_probation=%d" % (i + 1)) in t[1] for i, t in enumerate(tries)), str([t[1] for t in tries]))

        v = boot(ser, wait=7.0)  # the fourth start: the rollback switches slots and restarts once more
        check("the fourth start rolls back to the old image", v == "OLD-rollback-target", str(v))
        line = ask(ser, "ota", r"I ota:")
        check("the old image runs normally (no probation)", line is not None and "probation=0" in line, line or "")
        ser.write(b"log 12\n")
        time.sleep(2.5)
        log = ser.read(4000).decode("utf-8", "replace")
        check("the rollback is in the event log", "UpdateRolledBack" in log, "")

        # Self-test: an armed image that runs for 60 s confirms itself. Use the old image here; it has the same code.
        ask(ser, "ota arm", r"I ota:")
        time.sleep(66)
        line = ask(ser, "ota", r"I ota:")
        check("an image that runs 60 s with a working control loop confirms itself", line is not None and "probation=0" in line, line or "")
        ser.write(b"log 6\n")
        time.sleep(2.5)
        log = ser.read(4000).decode("utf-8", "replace")
        check("the confirmation is in the event log", "UpdateOk" in log, "")

    print("ALL PASS" if all(results) else "SOME FAILED")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
