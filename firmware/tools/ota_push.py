"""Push a firmware update to the device over Wi-Fi (cycle 021), the same way the panel does.

Usage:  python firmware/tools/ota_push.py <ip> [file] [--password PW]
  file      default: .pio/build/esp32dev/firmware.signed.bin (use firmware.bin for a device that has no signing key yet)
  password  default: environment SUNTRACKER_PASSWORD, else DEFAULT_PANEL_PASSWORD from src/secrets.h
It logs in, uploads, waits for the restart and checks that the device now reports the new version.
Needs only the Python standard library. Exit code 0 = new version is running.
"""
import argparse
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def default_password():
    if os.environ.get("SUNTRACKER_PASSWORD"):
        return os.environ["SUNTRACKER_PASSWORD"]
    try:
        text = open(os.path.join(ROOT, "src", "secrets.h"), encoding="utf-8").read()
        m = re.search(r'#define\s+DEFAULT_PANEL_PASSWORD\s+"([^"]*)"', text)
        return m.group(1) if m else ""
    except OSError:
        return ""


def call(url, data=None, headers=None, timeout=15):
    req = urllib.request.Request(url, data=data, headers=headers or {}, method="POST" if data is not None else "GET")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.headers, json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        try:
            body = json.loads(e.read() or b"{}")
        except ValueError:
            body = {}
        return e.code, e.headers, body


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ip")
    ap.add_argument("file", nargs="?", default=os.path.join(ROOT, ".pio", "build", "esp32dev", "firmware.signed.bin"))
    ap.add_argument("--password", default=default_password())
    a = ap.parse_args()
    base = "http://" + a.ip

    image = open(a.file, "rb").read()
    code, _, st = call(base + "/api/status")
    old = st.get("fwVersion", "?")
    print(f"device {a.ip} runs {old}; sending {os.path.basename(a.file)} ({len(image)} bytes)")

    code, hdr, body = call(base + "/api/login", json.dumps({"password": a.password}).encode(), {"Content-Type": "application/json"})
    cookie = (hdr.get("Set-Cookie") or "").split(";")[0]
    if code != 200 or not cookie:
        sys.exit(f"login failed ({code}): {body.get('message', body)}")

    boundary = "----stpush" + str(int(time.time()))
    head = f'--{boundary}\r\nContent-Disposition: form-data; name="firmware"; filename="firmware.bin"\r\nContent-Type: application/octet-stream\r\n\r\n'.encode()
    payload = head + image + f"\r\n--{boundary}--\r\n".encode()
    code, _, body = call(base + "/api/update", payload, {"Content-Type": "multipart/form-data; boundary=" + boundary, "Cookie": cookie}, timeout=120)
    if code != 200:
        sys.exit(f"update refused ({code}): {body.get('message', body)}")
    print("uploaded, waiting for the restart…")

    time.sleep(6)
    deadline = time.time() + 90
    while time.time() < deadline:
        try:
            _, _, st = call(base + "/api/status", timeout=4)
            new = st.get("fwVersion")
            if new:
                print(f"device now runs {new}" + (" (same version string as before)" if new == old else ""))
                print("It stays on probation for 60 s and rolls back by itself if it does not come up healthy.")
                return
        except (urllib.error.URLError, OSError, ValueError):
            pass
        time.sleep(2)
    sys.exit("the device did not answer after the update (it may be rolling back; check the log)")


if __name__ == "__main__":
    main()
