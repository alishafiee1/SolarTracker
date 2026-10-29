"use strict";
const $ = (id) => document.getElementById(id);
const fa = (n, d = 1) => Number(n).toLocaleString("fa-IR", { maximumFractionDigits: d, minimumFractionDigits: d });

let status = null;
let loggedIn = false;
let cities = [];

// ---- tiny API client ----
async function api(method, path, body) {
  const opt = { method, credentials: "same-origin", headers: {} };
  if (body !== undefined) { opt.body = JSON.stringify(body); opt.headers["Content-Type"] = "application/json"; }
  const r = await fetch(path, opt);
  let data = {};
  try { data = await r.json(); } catch (e) { /* empty body */ }
  if (r.status === 401 && data.error === "login-required") { setLoggedIn(false); }
  return { ok: r.ok, status: r.status, data };
}
function say(el, text, kind) { el.textContent = text || ""; el.className = "msg" + (kind ? " " + kind : ""); }
function errText(r) { return r.data.message || "خطا"; }

// ---- login ----
function setLoggedIn(v) {
  loggedIn = v;
  $("loginBox").classList.toggle("hidden", v);
  $("tabs").classList.toggle("hidden", !v);
  document.querySelectorAll("main section").forEach((s) => { if (!v && s.id !== "dash") s.classList.add("hidden"); });
  if (!v) showTab("dash");
}
$("loginBtn").onclick = async () => {
  const r = await api("POST", "/api/login", { password: $("loginPass").value });
  if (r.ok) { $("loginPass").value = ""; say($("loginMsg"), ""); setLoggedIn(true); loadAll(); }
  else say($("loginMsg"), errText(r), "err");
};
$("loginPass").onkeydown = (e) => { if (e.key === "Enter") $("loginBtn").click(); };

// ---- tabs ----
function showTab(name) {
  document.querySelectorAll("main section").forEach((s) => s.classList.toggle("hidden", s.id !== name));
  document.querySelectorAll("#tabs button").forEach((b) => b.classList.toggle("on", b.dataset.tab === name));
  if (name === "log") loadLog();
}
$("tabs").onclick = (e) => { const t = e.target.dataset && e.target.dataset.tab; if (t) showTab(t); };

// ---- status (every second) ----
function deviceDate(utc, tz) { return new Date((utc + tz * 60) * 1000).toISOString().replace("T", " ").slice(0, 19); }
async function poll() {
  try {
    const r = await api("GET", "/api/status");
    if (!r.ok) throw new Error("status");
    status = r.data;
    $("conn").textContent = "متصل"; $("conn").className = "pill";
    $("angle").textContent = fa(status.angleDeg) + "°";
    $("target").textContent = status.state === "Manual" ? "" : "هدف: " + fa(status.targetDeg) + "°";
    $("wind").textContent = fa(status.windKmh, 0);
    $("mode").textContent = status.mode === "manual" ? "دستی" : "اتو";
    $("state").textContent = status.state;
    $("fault").textContent = status.fault ? "خطا" : "—";
    $("faultText").textContent = status.fault ? status.text : "بدون خطا";
    $("statusText").textContent = status.text;
    $("fw").textContent = status.fwVersion;
    $("devTime").textContent = status.clockValid ? deviceDate(status.utc, status.tzOffsetMin) : "تنظیم نشده";
    $("warnDefault").classList.toggle("hidden", !status.passwordIsDefault);
    $("warnClock").classList.toggle("hidden", status.clockValid);
    $("warnCal").classList.toggle("hidden", status.calibrated);
    $("resetFault").classList.toggle("hidden", !status.fault || !loggedIn);
    $("clockNow").textContent = status.clockValid ? deviceDate(status.utc, status.tzOffsetMin) : "تنظیم نشده";
  } catch (e) {
    $("conn").textContent = "قطع"; $("conn").className = "pill off";
  }
}
setInterval(poll, 1000);

$("resetFault").onclick = async () => { await api("POST", "/api/fault/reset", {}); poll(); };

// ---- manual ----
$("modeAuto").onclick = async () => { const r = await api("POST", "/api/mode", { mode: "auto" }); say($("manualMsg"), r.ok ? "حالت اتوماتیک" : errText(r), r.ok ? "ok" : "err"); };
$("modeManual").onclick = async () => { const r = await api("POST", "/api/mode", { mode: "manual" }); say($("manualMsg"), r.ok ? "حالت دستی" : errText(r), r.ok ? "ok" : "err"); };
let keep = null;
async function move(dir) {
  const r = await api("POST", "/api/manual", { dir });
  say($("manualMsg"), r.ok ? "" : errText(r), r.ok ? "" : "err");
  if (r.ok && dir !== "stop" && !keep) keep = setInterval(() => api("POST", "/api/manual/keepalive", {}), 1000);
}
function release() { if (keep) { clearInterval(keep); keep = null; } api("POST", "/api/manual", { dir: "stop" }); }
for (const [id, dir] of [["moveEast", "east"], ["moveWest", "west"]]) {
  const b = $(id);
  b.onpointerdown = (e) => { e.preventDefault(); b.setPointerCapture(e.pointerId); move(dir); };
  b.onpointerup = release; b.onpointercancel = release; b.onlostpointercapture = release;
  b.oncontextmenu = (e) => e.preventDefault();
}
$("moveStop").onclick = release;

// ---- settings ----
const FIELDS = [
  ["moveThresholdDeg", "آستانه شروع حرکت (درجه)"], ["minDeg", "حداقل زاویه (درجه)"], ["maxDeg", "حداکثر زاویه (درجه)"],
  ["stallCheckMs", "زمان بررسی گیرکردن (میلی‌ثانیه)"], ["preDawnMinutes", "آماده‌سازی پیش از طلوع (دقیقه، ۰ = خاموش)"],
  ["stopSettleMs", "مکث بعد از توقف (میلی‌ثانیه)"], ["minMoveGapMs", "فاصله بین دو حرکت (میلی‌ثانیه)"], ["maxMoveMs", "سقف زمان یک حرکت (میلی‌ثانیه)"],
  ["windStowKmh", "آستانه باد شدید (km/h)"], ["windStowDelayS", "تأخیر ورود به حالت باد (ثانیه)"],
  ["windReturnKmh", "آستانه بازگشت از باد (km/h)"], ["windReturnMin", "مدت آرامش برای بازگشت (دقیقه)"],
  ["anemometerFactor", "ضریب بادسنج (km/h به ازای هر دور در ثانیه)"], ["supplyLowV", "هشدار ولتاژ تغذیه (ولت)"],
  ["currentPresent", "ماژول جریان نصب است (۱ = بله، ۰ = خیر)"], ["currentFactor", "ضریب ترانس جریان (آمپر به ازای هر ولت)"],
  ["stallCurrentA", "جریان گیرکردن (آمپر)"], ["maxCurrentA", "حداکثر جریان مجاز (آمپر)"], ["zeroCurrentA", "جریان «صفر» (آمپر)"],
];
let settingsData = {};
async function loadSettings() {
  const r = await api("GET", "/api/settings");
  if (!r.ok) return;
  settingsData = r.data;
  $("settingsForm").innerHTML = FIELDS.map(([k, label]) =>
    `<div class="field" id="f_${k}"><label for="i_${k}">${label}</label><input id="i_${k}" inputmode="decimal" value="${typeof settingsData[k] === "boolean" ? (settingsData[k] ? 1 : 0) : settingsData[k]}"></div>`).join("");
  $("locNow").textContent = `${settingsData.latDeg.toFixed(3)}, ${settingsData.lonDeg.toFixed(3)} (${settingsData.cityId})`;
  $("citySel").value = cities.some((c) => c.id === settingsData.cityId) ? settingsData.cityId : "";
}
$("saveSettings").onclick = async () => {
  const body = {};
  for (const [k] of FIELDS) {
    const v = $("i_" + k).value.trim().replace(/[٠-٩]/g, (d) => d.charCodeAt(0) - 1632).replace(/[۰-۹]/g, (d) => d.charCodeAt(0) - 1776);
    if (k === "currentPresent") { if (v !== "" && (v === "1") !== settingsData[k]) body[k] = v === "1"; continue; }
    if (v !== "" && Number(v) !== settingsData[k]) body[k] = Number(v);
  }
  document.querySelectorAll(".field.bad").forEach((e) => e.classList.remove("bad"));
  const r = await api("POST", "/api/settings", body);
  if (r.ok) { say($("settingsMsg"), "تنظیمات ذخیره شد", "ok"); loadSettings(); }
  else { say($("settingsMsg"), (errText(r) + (r.data.field ? ": " + r.data.field : "")), "err"); const f = $("f_" + r.data.field); if (f) f.classList.add("bad"); }
};

// ---- location ----
async function loadCities() {
  const r = await api("GET", "/api/cities");
  if (!r.ok) return;
  cities = r.data.cities;
  $("citySel").innerHTML = '<option value="">—</option>' + cities.map((c) => `<option value="${c.id}">${c.name}</option>`).join("");
}
$("saveCity").onclick = async () => {
  if (!$("citySel").value) return;
  const r = await api("POST", "/api/location", { city: $("citySel").value });
  say($("locMsg"), r.ok ? "موقعیت ذخیره شد" : errText(r), r.ok ? "ok" : "err"); if (r.ok) loadSettings();
};
$("saveCoord").onclick = async () => {
  const r = await api("POST", "/api/location", { text: $("coordText").value });
  say($("locMsg"), r.ok ? "موقعیت ذخیره شد" : errText(r), r.ok ? "ok" : "err"); if (r.ok) loadSettings();
};

// ---- clock ----
$("syncPhone").onclick = async () => {
  const r = await api("POST", "/api/clock", { utc: Math.floor(Date.now() / 1000) });
  say($("clockMsg"), r.ok ? "ساعت با گوشی همگام شد" : errText(r), r.ok ? "ok" : "err"); poll();
};
$("saveClock").onclick = async () => {
  const v = $("clockManual").value;
  if (!v || !status) return;
  const p = v.match(/(\d+)-(\d+)-(\d+)T(\d+):(\d+):?(\d*)/);
  const local = Date.UTC(+p[1], +p[2] - 1, +p[3], +p[4], +p[5], +(p[6] || 0)) / 1000;  // entered in the device's local time
  const r = await api("POST", "/api/clock", { utc: local - status.tzOffsetMin * 60 });
  say($("clockMsg"), r.ok ? "ساعت ثبت شد" : errText(r), r.ok ? "ok" : "err"); poll();
};

// ---- calibration ----
const CAL = { pending: "در حال ثبت… پنل را ثابت نگه دارید", ok: "ثبت شد", moving: "کالیبراسیون نامعتبر: پنل در حال حرکت بود", "too-small-angle": "کالیبراسیون نامعتبر: زاویه دو مرحله خیلی کم است",
  "too-large-angle": "کالیبراسیون نامعتبر: زاویه دو مرحله خیلی زیاد است", "bad-gravity": "کالیبراسیون نامعتبر: داده سنسور درست نیست", "sensor-fault": "سنسور زاویه پاسخ نمی‌دهد", "not-captured": "" };
async function capture(step) {
  const r = await api("POST", "/api/calibrate/" + step, {});
  if (!r.ok) { say($("calMsg"), errText(r), "err"); return; }
  for (let i = 0; i < 40; i++) {
    const s = await api("GET", "/api/calibrate");
    say($("calMsg"), CAL[s.data.result] || "", s.data.result === "pending" ? "" : (s.data.result === "ok" ? "ok" : "err"));
    if (s.data.result !== "pending") return;
    await new Promise((res) => setTimeout(res, 500));
  }
}
$("calH").onclick = () => capture("horizontal");
$("calE").onclick = () => capture("east");
$("calSave").onclick = async () => { const r = await api("POST", "/api/calibrate/save", {}); say($("calMsg"), r.ok ? "کالیبراسیون ذخیره شد" : errText(r), r.ok ? "ok" : "err"); poll(); };

// ---- log ----
async function loadLog() {
  const r = await api("GET", "/api/log?n=100");
  if (!r.ok) return;
  const tz = status ? status.tzOffsetMin : 0;
  $("logRows").innerHTML = r.data.events.slice().reverse().map((e) =>
    `<tr><td class="t">${e.t > 0 ? deviceDate(e.t, tz) : "—"}</td><td>${e.type}</td><td>${String(e.detail).replace(/[<&]/g, "")}</td></tr>`).join("");
}
$("logReload").onclick = loadLog;

// ---- password ----
$("pwSave").onclick = async () => {
  const r = await api("POST", "/api/password", { old: $("pwOld").value, new: $("pwNew").value });
  if (r.ok) { say($("pwMsg"), "رمز عوض شد؛ دوباره وارد شوید", "ok"); $("pwOld").value = ""; $("pwNew").value = ""; setLoggedIn(false); }
  else say($("pwMsg"), errText(r), "err");
};

async function loadAll() { await loadCities(); await loadSettings(); }

(async () => {
  await poll();
  const r = await api("GET", "/api/settings");  // 401 when there is no session yet
  setLoggedIn(r.ok);
  if (r.ok) loadAll();
})();
