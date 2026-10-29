"""Hardware check for cycle 005 on a real ESP32 flashed with env esp32dev-bench.

Usage: python firmware/tools/bench_check_005.py COM7
Without an MPU6050 the state machine must latch an angle-sensor fault, keep the relays off, refuse manual
commands and latch the fault again after a reset while the sensor is still missing (R6, R7, R8).
The closed loop with a real panel needs the sensor and an actuator and is done by hand with `ctl ...`.
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


def main():
    with serial.Serial(sys.argv[1], 115200, timeout=0.3) as ser:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        time.sleep(4.0)

        line = ask(ser, "ctl", r"I ctl:")
        check("R7 no sensor: state machine is in Fault", line is not None and "state=Fault" in line and "fault=angle-sensor" in line, line or "")
        line = ask(ser, "pins", r"I pins:")
        check("relays stay off", line is not None and "CH1(power)=1 CH2(direction)=1" in line, line or "")

        ask(ser, "ctl manual", r"I ctl:")
        line = ask(ser, "ctl w", r"I ctl:")
        check("manual command refused during a fault (FaultActive = 3)", line is not None and "result=3" in line, line or "")
        line = ask(ser, "pins", r"I pins:")
        check("relays still off after the refused command", line is not None and "CH1(power)=1 CH2(direction)=1" in line, line or "")

        line = ask(ser, "ctl reset", r"I ctl:")
        time.sleep(1.5)
        line = ask(ser, "ctl", r"I ctl:")
        check("R8 reset with the sensor still missing: the fault comes back", line is not None and "fault=angle-sensor" in line, line or "")
        ask(ser, "ctl auto", r"I ctl:")

    print("ALL PASS" if all(results) else "SOME FAILED")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
