"""Read the device serial log for a few seconds (optionally resetting the board first).

Usage: python firmware/tools/serial_log.py COM7 [seconds] [--reset]
Needs pyserial (bundled with PlatformIO's Python: `pio pkg exec -- python ...`).
"""
import sys
import time

import serial


def main():
    port = sys.argv[1]
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else 5.0
    reset = "--reset" in sys.argv
    with serial.Serial(port, 115200, timeout=0.2) as ser:
        if reset:
            ser.dtr = False
            ser.rts = True  # EN low
            time.sleep(0.1)
            ser.rts = False
        end = time.time() + seconds
        while time.time() < end:
            line = ser.readline()
            if line:
                print(line.decode("utf-8", "replace").rstrip())


if __name__ == "__main__":
    main()
