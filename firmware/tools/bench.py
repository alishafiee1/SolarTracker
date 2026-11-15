"""Talk to the device over USB serial (bench image) or just read its log. One tool instead of a script per cycle.

Usage:
  python firmware/tools/bench.py COM8 status              send one command, print the answer
  python firmware/tools/bench.py COM8 "angle" "motion"    several commands in a row
  python firmware/tools/bench.py COM8 --log 10            only read the log for 10 s (works on the normal image too)
  python firmware/tools/bench.py COM8 --reset status      reset the board first (EN pin), then send commands
Commands exist only in the esp32dev-bench image; see docs/serial.md. Needs pyserial (`pio pkg exec -- python ...`).
Answer lines start with "I " (info) or "E " (error); the tool stops reading when the line stays quiet for --quiet seconds.
"""
import argparse
import sys
import time

import serial


def read_until_quiet(ser, quiet, limit):
    lines, last, end = [], time.time(), time.time() + limit
    while time.time() < end and time.time() - last < quiet:
        raw = ser.readline()
        if raw:
            last = time.time()
            text = raw.decode("utf-8", "replace").rstrip()
            if "Wire.cpp" not in text:  # known noise from the I2C driver when a sensor is missing
                lines.append(text)
    return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("commands", nargs="*")
    ap.add_argument("--log", type=float, metavar="SECONDS", help="only read the log for this long")
    ap.add_argument("--reset", action="store_true", help="reset the board before anything else")
    ap.add_argument("--quiet", type=float, default=0.8, help="seconds of silence that end an answer")
    a = ap.parse_args()
    with serial.Serial(a.port, 115200, timeout=0.2) as ser:
        if a.reset:
            ser.dtr = False
            ser.rts = True
            time.sleep(0.1)
            ser.rts = False
            time.sleep(1.5)
        if a.log is not None:
            end = time.time() + a.log
            while time.time() < end:
                raw = ser.readline()
                if raw:
                    print(raw.decode("utf-8", "replace").rstrip())
            return
        if not a.commands:
            sys.exit("give a command (try: status, help) or --log N")
        for cmd in a.commands:
            ser.reset_input_buffer()
            ser.write((cmd + "\n").encode())
            print("> " + cmd)
            for line in read_until_quiet(ser, a.quiet, 10):
                print(line)


if __name__ == "__main__":
    main()
