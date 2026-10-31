"""Desktop simulator of the SunTracker device: the real firmware logic (build: pio run -e sim) behind the real panel files.

Run:    python firmware/sim/sim_server.py [--port 8080] [--speed 1]
Open:   http://127.0.0.1:8080/        (panel, login password: sim)
        http://127.0.0.1:8080/sim     (simulator controls: wind, faults, time speed)
No hardware is touched. The simulated day starts at 07:00 UTC (10:30 in Kerman) on 21 June 2027.
"""
import argparse
import json
import mimetypes
import os
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

HERE = os.path.dirname(os.path.abspath(__file__))
FW = os.path.dirname(HERE)
WEB = os.path.join(FW, "web")
EXE = os.path.join(FW, ".pio", "build", "sim", "program.exe" if os.name == "nt" else "program")


class Sim:
    def __init__(self):
        self.p = subprocess.Popen([EXE], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1, encoding="utf-8")
        self.lock = threading.Lock()
        assert json.loads(self.p.stdout.readline()).get("ready"), "simulator did not start"
        self.speed = 1.0

    def call(self, msg):
        with self.lock:
            self.p.stdin.write(json.dumps(msg) + "\n")
            self.p.stdin.flush()
            return json.loads(self.p.stdout.readline())


SIM = None


def clock_thread():
    """Advance the simulated device: speed x real time, in 100 ms control ticks."""
    t0 = time.monotonic()
    done = 0.0
    while True:
        time.sleep(0.05)
        want = (time.monotonic() - t0) * SIM.speed * 10.0  # 10 ticks of 100 ms per real second at speed 1
        n = int(want - done)
        if n > 0:
            SIM.call({"op": "step", "steps": n})
            done += n
        if SIM.speed != getattr(clock_thread, "last", SIM.speed):
            t0, done = time.monotonic(), 0.0
        clock_thread.last = SIM.speed


SIM_PAGE = """<!doctype html><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>شبیه‌ساز</title><style>body{font:15px system-ui;max-width:560px;margin:16px auto;padding:0 12px;direction:rtl}
fieldset{margin:10px 0;border:1px solid #8886;border-radius:8px}label{display:block;margin:6px 0}pre{background:#8882;padding:8px;border-radius:6px;direction:ltr}
button{padding:6px 12px;margin:2px}</style>
<h2>شبیه‌ساز دستگاه</h2><p><a href="/">باز کردن پنل</a> (رمز: sim)</p>
<fieldset><legend>سرعت زمان</legend>
<button onclick="spd(1)">×۱</button><button onclick="spd(10)">×۱۰</button><button onclick="spd(60)">×۶۰</button><button onclick="spd(300)">×۳۰۰</button><button onclick="spd(0)">توقف</button></fieldset>
<fieldset><legend>باد</legend><label>سرعت باد: <b id=wv>0</b> km/h <input id=w type=range min=0 max=90 value=0 oninput="wv.textContent=this.value;set({windKmh:+this.value})"></label></fieldset>
<fieldset><legend>خرابی‌ها</legend>
<label><input type=checkbox onchange="set({jammed:this.checked})"> بازو گیر کرده</label>
<label><input type=checkbox onchange="set({wireBroken:this.checked})"> سیم جک قطع</label>
<label><input type=checkbox onchange="set({relayStuck:this.checked})"> رله چسبیده</label>
<label><input type=checkbox onchange="set({sensorUnplugged:this.checked})"> سنسور زاویه جدا</label>
<label><input type=checkbox onchange="set({clockValid:!this.checked})"> ساعت نامعتبر</label>
<label><input type=checkbox onchange="set({currentModule:this.checked})"> ماژول جریان نصب</label></fieldset>
<fieldset><legend>وضعیت واقعی (فیزیک)</legend><pre id=st></pre></fieldset>
<script>
async function set(o){await fetch('/sim/set',{method:'POST',body:JSON.stringify(o)})}
async function spd(v){await fetch('/sim/speed',{method:'POST',body:JSON.stringify({speed:v})})}
async function tick(){try{const s=await (await fetch('/sim/state')).json();s.time=new Date(s.utc*1000).toISOString();st.textContent=JSON.stringify(s,null,1)}catch(e){}setTimeout(tick,1000)}tick()
</script>"""


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, code, body, ctype="application/json", headers=()):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        for k, v in headers:
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def handle_any(self, method):
        u = urlsplit(self.path)
        path = u.path
        n = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(n).decode("utf-8") if n else ""
        if path.startswith("/api/"):
            r = SIM.call({"op": "api", "method": method, "path": path, "query": u.query, "body": body, "cookie": self.headers.get("Cookie", "")})
            if r.get("csv"):
                self.send(200, r["body"], "text/csv; charset=utf-8", [("Content-Disposition", 'attachment; filename="suntracker-log.csv"')])
                return
            hdr = [("Set-Cookie", r["cookie"])] if r.get("cookie") else []
            self.send(r.get("status", 500), r.get("body", "{}"), "application/json", hdr)
        elif path == "/sim":
            self.send(200, SIM_PAGE, "text/html; charset=utf-8")
        elif path == "/sim/state":
            self.send(200, json.dumps(SIM.call({"op": "state"})))
        elif path == "/sim/set" and method == "POST":
            self.send(200, json.dumps(SIM.call({"op": "sim", "set": json.loads(body or "{}")})))
        elif path == "/sim/speed" and method == "POST":
            SIM.speed = float(json.loads(body or "{}").get("speed", 1))
            self.send(200, '{"ok":true}')
        else:
            name = "index.html" if path in ("/", "/index.html") else path.lstrip("/")
            f = os.path.normpath(os.path.join(WEB, name))
            if f.startswith(WEB) and os.path.isfile(f):
                with open(f, "rb") as fh:
                    self.send(200, fh.read(), (mimetypes.guess_type(f)[0] or "application/octet-stream") + "; charset=utf-8")
            else:  # captive-portal style: unknown pages go to the panel
                self.send(302, "", "text/plain", [("Location", "/")])

    def do_GET(self):
        self.handle_any("GET")

    def do_POST(self):
        self.handle_any("POST")


def main():
    global SIM
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--speed", type=float, default=1.0, help="simulated seconds per real second")
    a = ap.parse_args()
    if not os.path.exists(EXE):
        sys.exit("simulator not built: run  pio run -d firmware -e sim")
    SIM = Sim()
    SIM.speed = a.speed
    threading.Thread(target=clock_thread, daemon=True).start()
    srv = ThreadingHTTPServer(("127.0.0.1", a.port), Handler)
    print(f"panel      http://127.0.0.1:{a.port}/   (password: sim)\ncontrols   http://127.0.0.1:{a.port}/sim", flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
