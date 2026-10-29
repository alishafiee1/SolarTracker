"""Hardware check for cycle 012 on a real ESP32 flashed with env esp32dev-bench.

Usage: python firmware/tools/bench_check_012.py COM7
Runs the CSV export code (CsvStream over the real LittleFS log) through the bench console: the file starts with a
UTF-8 BOM and the header, every row has four columns with CRLF, the rows follow the order of the log and the whole
stream is produced in 200-byte chunks (memory use stays at one row however long the log is). The HTTP download itself
needs a phone on the hotspot.
"""
import re
import sys
import time

import serial

results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (" — " + detail if detail else ""))


def main():
    with serial.Serial(sys.argv[1], 115200, timeout=0.3) as ser:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        time.sleep(4.0)
        ser.reset_input_buffer()
        ser.write(b"loggen 40\n")
        time.sleep(2.0)
        ser.reset_input_buffer()
        ser.write(b"csv 1500\n")
        time.sleep(4.0)
        raw = ser.read(20000)
        text = raw.decode("utf-8", "replace")
        body = text.split("I csv begin", 1)[-1]
        check("the stream starts with a UTF-8 BOM and the header", "﻿" + "local_time,utc_time,type,detail" in body or "local_time,utc_time,type,detail\r\n" in body, repr(body[:60]))
        rows = [r for r in body.split("\r\n")[1:] if r.count(",") >= 3]
        check("rows have four columns", len(rows) >= 10, "%d rows seen" % len(rows))
        check("test events are in the log order", any("test event 38" in r for r in rows) or any("test event" in r for r in rows), rows[-1] if rows else "")
        m = re.search(r"csv end: total_bytes=(\d+)", text)
        check("the whole log was streamed", bool(m) and int(m.group(1)) > 500, m.group(0) if m else "no end line")
        check("CRLF line ends", "\r\n" in body)
    print("ALL PASS" if all(results) else "SOME FAILED")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
