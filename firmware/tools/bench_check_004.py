"""Hardware check for cycle 004 on a real ESP32 flashed with env esp32dev-bench.

Usage: python firmware/tools/bench_check_004.py COM7
Run it with the actuators DISCONNECTED (relay inputs only, or nothing wired). It drives the
real MotionController and polls the relay pin levels over serial (resolution about 30-50 ms):
  - the first move cannot start within 3 s of boot (R9)
  - CH2 (direction) is set first, CH1 (power) follows after about 200 ms (R2)
  - a stop switches CH1 off at once (R3)
  - a reverse request never changes CH2 while CH1 is on and waits for the safety gap (R4, R5)
Levels: 1 = relay input inactive (high), 0 = active (low).
"""
import re
import sys
import time

import serial

results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (" — " + detail if detail else ""))


class Bench:
    def __init__(self, ser):
        self.ser = ser
        self.t0 = time.time()
        self.trace = []  # (seconds, ch1, ch2)

    def cmd(self, text):
        self.ser.write((text + "\n").encode())

    def poll(self):
        self.cmd("pins")
        end = time.time() + 0.3
        while time.time() < end:
            line = self.ser.readline().decode("utf-8", "replace")
            m = re.search(r"CH1\(power\)=(\d) CH2\(direction\)=(\d)", line)
            if m:
                self.trace.append((time.time() - self.t0, int(m.group(1)), int(m.group(2))))
                return self.trace[-1]
        return None

    def watch(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            self.poll()


def first(trace, pred, after=0.0):
    for t, a, b in trace:
        if t >= after and pred(a, b):
            return t
    return None


def main():
    with serial.Serial(sys.argv[1], 115200, timeout=0.05) as ser:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        boot = time.time()
        b = Bench(ser)
        b.t0 = boot
        time.sleep(0.5)

        # R9: ask for a move right after boot.
        b.cmd("move w")
        b.watch(2.0)
        early_power = first(b.trace, lambda p, d: p == 0)
        check("R9 no power in the first 2 s after boot", early_power is None)

        # Wait for the start sequence.
        b.watch(4.0)
        t_dir = first(b.trace, lambda p, d: d == 0)
        t_pow = first(b.trace, lambda p, d: p == 0)
        check("R9 first move starts only after the 3 s settle", t_dir is not None and t_dir >= 2.9, "CH2 at %.2f s" % (t_dir or -1))
        check("R2 direction first, power after about 200 ms", t_dir is not None and t_pow is not None and 0.1 <= t_pow - t_dir <= 0.45, "gap %.2f s" % ((t_pow or 0) - (t_dir or 0)))

        # R3: stop switches CH1 off at once.
        n_before = len(b.trace)
        t_stop = time.time() - b.t0  # the moment the stop is requested (the move ends in the next tick)
        b.cmd("stop")
        b.watch(1.0)
        t_off = first(b.trace[n_before:], lambda p, d: p == 1)
        check("R3 stop switches CH1 off within 0.3 s", t_off is not None and t_off - b.trace[n_before][0] < 0.3)
        check("R3 CH2 stays unchanged at stop", b.trace[-1][2] == 0)

        # R5/R4: ask for the opposite direction right away; it must wait for the 10 s gap.
        b.cmd("move e")
        n_rev = len(b.trace)
        b.watch(7.0)
        t_early = first(b.trace[n_rev:], lambda p, d: p == 0)
        check("R5 no power within 7 s of the previous move end", t_early is None)
        b.watch(7.0)
        t_dir2 = first(b.trace[n_rev:], lambda p, d: d == 1)
        t_pow2 = first(b.trace[n_rev:], lambda p, d: p == 0)
        check("R4 CH2 switched with CH1 off", t_dir2 is not None and all(p == 1 for t, p, d in b.trace[n_rev:] if t <= (t_dir2 or 0) and t >= (t_dir2 or 0) - 0.1), "CH2 at %.2f s" % (t_dir2 or -1))
        check("R5 reverse started at least 10 s after the stop", t_pow2 is not None and t_pow2 - t_stop >= 9.8, "%.2f s after the stop" % ((t_pow2 or 0) - t_stop))
        b.cmd("stop")
        b.watch(0.5)

    print("ALL PASS" if all(results) else "SOME FAILED")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
