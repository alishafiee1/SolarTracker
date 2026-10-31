"use strict";
const $ = (id) => document.getElementById(id);
const fa = (n, d = 1) => Number(n).toLocaleString("fa-IR", { maximumFractionDigits: d, minimumFractionDigits: d });

// ---- help texts: one per field. min/max/def must match the validation in the firmware (a host test compares them) ----
const HELP = /*HELP-BEGIN*/{
  "moveThresholdDeg": {"t": "کمترین اختلاف بین زاویه پنل و زاویه‌ای که خورشید می‌خواهد تا دستگاه حرکت کند. عدد کم‌تر یعنی ردیابی دقیق‌تر ولی حرکت و قطع و وصل رله بیشتر؛ عدد بیشتر یعنی حرکت کمتر و خطای ردیابی بیشتر. با ۵ درجه فقط حدود ۰٫۴٪ تولید از دست می‌رود.", "min": 2, "max": 15, "def": 5, "unit": "درجه"},
  "minDeg": {"t": "انتهای شرقی بازه چرخش پنل (منفی یعنی شرق). پنل هیچ‌وقت بیشتر از این به شرق نمی‌رود. باید با محدودیت مکانیکی سازه و جک هماهنگ باشد و از حداکثر کمتر باشد.", "min": -60, "max": 0, "def": -45, "unit": "درجه"},
  "maxDeg": {"t": "انتهای غربی بازه چرخش پنل (مثبت یعنی غرب). پنل هیچ‌وقت بیشتر از این به غرب نمی‌رود. باید با محدودیت مکانیکی سازه و جک هماهنگ باشد و از حداقل بیشتر باشد.", "min": 0, "max": 60, "def": 45, "unit": "درجه"},
  "preDawnMinutes": {"t": "اگر بیشتر از صفر باشد، این‌قدر دقیقه پیش از طلوع پنل به سمت شرق می‌رود تا اولین نور را بگیرد. صفر یعنی خاموش: پنل تا طلوع افقی می‌ماند.", "min": 0, "max": 120, "def": 0, "unit": "دقیقه"},
  "stallCheckMs": {"t": "بعد از شروع حرکت، دستگاه این‌قدر صبر می‌کند و بعد می‌بیند زاویه دست‌کم یک درجه تغییر کرده یا نه. اگر نه، بازو گیر کرده فرض می‌شود و بعد از سه تلاش خطا می‌دهد. جک کند به زمان بیشتری نیاز دارد؛ عدد خیلی کم خطای کاذب می‌دهد.", "min": 2000, "max": 20000, "def": 5000, "unit": "میلی‌ثانیه"},
  "stopSettleMs": {"t": "بعد از هر توقف، این‌قدر صبر می‌شود تا موتور کامل بایستد و بعد حرکت بعدی اجازه دارد. کم کردنش به جک و سازه ضربه می‌زند. برای تغییر جهت جریان باید صفر و زاویه ثابت هم باشد.", "min": 1000, "max": 10000, "def": 3000, "unit": "میلی‌ثانیه"},
  "minMoveGapMs": {"t": "حداقل فاصله بین پایان یک حرکت و شروع حرکت بعدی، در همه حالت‌ها (حتی دستی و باد). رله و موتور را از قطع و وصل پیاپی نگه می‌دارد. پیشنهاد: کم نکنید. تغییر جهت هم دست‌کم این‌قدر طول می‌کشد.", "min": 1000, "max": 60000, "def": 10000, "unit": "میلی‌ثانیه"},
  "maxMoveMs": {"t": "بیشترین مدت یک حرکت پیوسته. اگر تا آن موقع به هدف نرسد، برق جک قطع و خطای «زمان حرکت تمام شد» ثبت می‌شود. کورس کامل ۹۰ درجه با سرعت ۰٫۶ درجه بر ثانیه حدود ۱۵۰ ثانیه است؛ اگر جک شما کندتر است این عدد را بیشتر کنید.", "min": 10000, "max": 300000, "def": 120000, "unit": "میلی‌ثانیه"},
  "windStowKmh": {"t": "اگر سرعت باد از این عدد بیشتر شود و برای «تأخیر ورود» بماند، پنل افقی می‌شود تا باد آسیب نزند. عدد کمتر یعنی محافظت بیشتر ولی توقف ردیابی در بادهای ملایم‌تر.", "min": 10, "max": 100, "def": 40, "unit": "km/h"},
  "windStowDelayS": {"t": "باد باید این‌قدر ثانیه پشت سر هم بالاتر از آستانه بماند تا پنل افقی شود؛ تندبادهای کوتاه نادیده گرفته می‌شوند.", "min": 1, "max": 60, "def": 5, "unit": "ثانیه"},
  "windReturnKmh": {"t": "بعد از باد شدید، فقط وقتی باد زیر این عدد باشد شمارش بازگشت به ردیابی شروع می‌شود. باید از آستانه باد شدید کمتر باشد.", "min": 5, "max": 99, "def": 30, "unit": "km/h"},
  "windReturnMin": {"t": "باد باید این‌قدر دقیقه پیوسته زیر «آستانه بازگشت» بماند تا ردیابی دوباره شروع شود. هر تندباد بالاتر از آستانه بازگشت، شمارش را از اول می‌کند.", "min": 1, "max": 120, "def": 15, "unit": "دقیقه"},
  "anemometerFactor": {"t": "ضریب تبدیل دور بادسنج به سرعت: چند کیلومتر بر ساعت به ازای هر دور در ثانیه. برای بادسنج ۲۰ سانتی حدود ۶٫۸ است. برای دقت بیشتر در محل نصب با یک بادسنج دستی مقایسه و تنظیم کنید.", "min": 1, "max": 20, "def": 6.8, "unit": "km/h به ازای هر دور در ثانیه"},
  "supplyLowV": {"t": "اگر ولتاژ تغذیه ۵ ولت دستگاه برای ۳ ثانیه زیر این عدد برود، رویداد «ولتاژ پایین» در لاگ ثبت می‌شود. فقط هشدار است و دستگاه را متوقف نمی‌کند.", "min": 3, "max": 6, "def": 4.5, "unit": "ولت"},
  "currentPresent": {"t": "اگر ترانس جریان (SCT-013) روی دستگاه نصب است ۱ بگذارید تا گیرکردن، قطع سیم، رله چسبیده و اضافه جریان با جریان واقعی تشخیص داده شود. بدون ترانس حتماً ۰ بماند، وگرنه جریان صفر خوانده می‌شود و خطای قطع سیم می‌دهد.", "min": 0, "max": 1, "def": 0, "unit": "۰ یا ۱"},
  "currentFactor": {"t": "ضریب تبدیل ولتاژ ترانس به آمپر (آمپر به ازای هر ولت rms). برای SCT-013 مدل ۵ آمپری حدود ۵ است. با یک آمپرمتر انبری روی سیم جک کالیبره کنید.", "min": 0.1, "max": 50, "def": 5, "unit": "آمپر بر ولت"},
  "stallCurrentA": {"t": "اگر جریان بالاتر از این عدد باشد و زاویه تغییر نکند، بازو گیر کرده فرض می‌شود. باید بیشتر از جریان کار عادی (حدود ۳ آمپر برای دو جک) و کمتر از حداکثر جریان باشد.", "min": 0.5, "max": 20, "def": 4.5, "unit": "آمپر"},
  "maxCurrentA": {"t": "اگر جریان بیش از ۰٫۳ ثانیه از این عدد بالاتر برود، برق فوراً قطع و خطای اضافه جریان ثبت می‌شود. جریان لحظه شروع (هجومی) در ۰٫۳ ثانیه اول حساب نمی‌شود. باید از جریان گیرکردن بیشتر باشد.", "min": 1, "max": 30, "def": 5, "unit": "آمپر"},
  "zeroCurrentA": {"t": "جریان کمتر از این عدد «بدون جریان» حساب می‌شود. وقتی برق وصل است و ۱ ثانیه جریان نیست، یا انتهای کورس جک است (نزدیک حد زاویه) یا سیم قطع شده است.", "min": 0.05, "max": 1, "def": 0.2, "unit": "آمپر"},
  "apPassword": {"t": "رمز وای‌فای هات‌اسپات دستگاه (۸ تا ۳۲ نویسه). بعد از راه‌اندازی دوباره دستگاه اعمال می‌شود و هرگز نمایش داده نمی‌شود. خالی بگذارید تا تغییر نکند."},
  "city": {"t": "شهر نصب را از فهرست مراکز استان انتخاب کنید؛ مختصات آن برای حساب جای خورشید به کار می‌رود. خطای چند ده کیلومتر روی ردیابی اثر محسوسی ندارد."},
  "coords": {"t": "مختصات دقیق را از نقشه کپی کنید، به شکل «عرض، طول» مثل 30.283, 57.083. عرض بین ۹۰- و ۹۰ و طول بین ۱۸۰- و ۱۸۰ است (شرق مثبت)."},
  "clock": {"t": "ساعت محلی را وارد کنید؛ دستگاه با منطقه زمانی تنظیم‌شده آن را به UTC تبدیل می‌کند. ساده‌تر این است که دکمه «همگام‌سازی با ساعت گوشی» را بزنید."},
  "netSsid": {"t": "نام وای‌فای محل نصب (فقط ۲٫۴ گیگاهرتز). از فهرست جستجو انتخاب کنید یا تایپ کنید؛ حروف بزرگ و کوچک مهم‌اند."},
  "netPass": {"t": "رمز وای‌فای محل نصب (دست‌کم ۸ نویسه؛ خالی یعنی شبکه باز). هرگز نمایش داده نمی‌شود؛ اگر خالی بگذارید رمز قبلی می‌ماند."},
  "botToken": {"t": "توکن ربات بله که از BotFather می‌گیرید. نمایش داده نمی‌شود و فقط روی دستگاه ذخیره می‌شود؛ خالی بگذارید تا تغییر نکند."},
  "chatId": {"t": "شناسه (عدد) گفتگویی که هشدارها به آن فرستاده می‌شود."},
  "notifyOn": {"t": "اگر روشن باشد، خطاها و شروع باد شدید به ربات بله فرستاده می‌شوند. به وای‌فای محل و اینترنت نیاز دارد."},
  "pwNew": {"t": "رمز جدید پنل (دست‌کم ۸ نویسه). بعد از تغییر همه نشست‌ها بسته می‌شوند و باید دوباره وارد شوید. رمز پیش‌فرض همه دستگاه‌ها یکی است و باید عوض شود."}
}/*HELP-END*/;

function helpHtml(key) {
  const h = HELP[key];
  if (!h) return "";
  let s = h.t;
  if (h.min !== undefined && key !== "currentPresent") s += `<br><b>بازه مجاز:</b> ${fa(h.min, h.min % 1 ? 2 : 0)} تا ${fa(h.max, h.max % 1 ? 2 : 0)} ${h.unit || ""} · <b>پیش‌فرض:</b> ${fa(h.def, h.def % 1 ? 1 : 0)}`;
  else if (h.unit) s += `<br><b>پیش‌فرض:</b> ${fa(h.def, 0)} (${h.unit})`;
  return s;
}
function bindHelp(root) {
  root.querySelectorAll("button.help").forEach((b) => {
    b.onclick = (e) => {
      e.preventDefault();
      const box = $("h_" + b.dataset.help);
      if (!box) return;
      box.innerHTML = helpHtml(b.dataset.help);
      box.classList.toggle("hidden");
    };
  });
}

// ---- small UI helpers ----
function toast(text, kind) {
  const t = document.createElement("div");
  t.className = "toast" + (kind ? " " + kind : "");
  t.textContent = text;
  $("toasts").appendChild(t);
  setTimeout(() => t.remove(), kind === "err" ? 6000 : 3500);
}
function say(el, text, kind) { el.textContent = text || ""; el.className = "msg" + (kind ? " " + kind : ""); }
function confirmDialog(text) {
  const d = $("confirmBox");
  if (!d.showModal) return Promise.resolve(window.confirm(text));
  $("confirmText").textContent = text;
  return new Promise((res) => {
    $("confirmOk").onclick = () => { d.close(); res(true); };
    $("confirmNo").onclick = () => { d.close(); res(false); };
    d.showModal();
  });
}
function errText(r) { return (r.data && r.data.message) || "خطا"; }

let status = null, loggedIn = false, cities = [], settingsData = {}, limits = { min: -45, max: 45 };

async function api(method, path, body) {
  const opt = { method, credentials: "same-origin", headers: {} };
  if (body !== undefined) { opt.body = JSON.stringify(body); opt.headers["Content-Type"] = "application/json"; }
  const r = await fetch(path, opt);
  let data = {};
  try { data = await r.json(); } catch (e) { /* empty body */ }
  if (r.status === 401 && data.error === "login-required") setLoggedIn(false);
  return { ok: r.ok, status: r.status, data };
}

// ---- tabs ----
const TABS = [["dash", "وضعیت", "وضعیت"], ["manual", "دستی", "کنترل"], ["calib", "کالیبراسیون", "کنترل"], ["settings", "تنظیمات", "تنظیمات"],
  ["location", "موقعیت", "تنظیمات"], ["clock", "ساعت", "تنظیمات"], ["network", "شبکه", "تنظیمات"], ["log", "لاگ", "ابزار"], ["update", "به‌روزرسانی", "ابزار"], ["account", "رمز", "ابزار"]];
function buildTabs() {
  let html = "", last = "";
  for (const [id, label, group] of TABS) {
    if (group !== last) { html += `<span class="grp">${group}</span>`; last = group; }
    html += `<button data-tab="${id}"${id === "dash" ? ' class="on"' : ""}>${label}</button>`;
  }
  $("tabs").innerHTML = html;
}
function showTab(name) {
  if (name !== "dash" && !loggedIn) name = "dash";
  document.querySelectorAll("main section").forEach((s) => s.classList.toggle("hidden", s.id !== name));
  document.querySelectorAll("#tabs button").forEach((b) => b.classList.toggle("on", b.dataset.tab === name));
  if (name === "log") loadLog();
  if (name === "network") loadNetwork();
}
$("tabs").onclick = (e) => { const t = e.target.dataset && e.target.dataset.tab; if (t) showTab(t); };
buildTabs();

// ---- login ----
function setLoggedIn(v) {
  const changed = loggedIn !== v;
  loggedIn = v;
  $("loginBox").classList.toggle("hidden", v);
  $("tabs").classList.toggle("hidden", !v);
  $("logoutBtn").classList.toggle("hidden", !v);
  if (!v) showTab("dash");
  if (changed && v) loadAll();
}
$("loginBtn").onclick = async () => {
  const r = await api("POST", "/api/login", { password: $("loginPass").value });
  if (r.ok) { $("loginPass").value = ""; say($("loginMsg"), ""); setLoggedIn(true); toast("وارد شدید", "ok"); }
  else say($("loginMsg"), errText(r), "err");
};
$("loginPass").onkeydown = (e) => { if (e.key === "Enter") $("loginBtn").click(); };
$("logoutBtn").onclick = async () => { await api("POST", "/api/logout", {}); setLoggedIn(false); };

// ---- gauge ----
const NS = "http://www.w3.org/2000/svg";
const pt = (a, r) => { const t = (a * Math.PI) / 180; return [160 + r * Math.sin(t), 150 - r * Math.cos(t)]; };
function buildGauge() {
  const [x1, y1] = pt(limits.min, 118), [x2, y2] = pt(limits.max, 118);
  const lab = (a, txt) => { const [x, y] = pt(a, 138); return `<text x="${x}" y="${y}" text-anchor="middle" font-size="12" fill="var(--mute)">${txt}</text>`; };
  $("gauge").innerHTML =
    `<rect x="0" y="0" width="320" height="190" rx="12" fill="var(--sky)"/>` +
    `<path d="M${x1} ${y1} A118 118 0 0 1 ${x2} ${y2}" fill="none" stroke="var(--line)" stroke-width="10" stroke-linecap="round"/>` +
    lab(limits.min, fa(limits.min, 0) + "°") + lab(0, "۰°") + lab(limits.max, "+" + fa(limits.max, 0) + "°") +
    `<text x="14" y="176" font-size="13" fill="var(--mute)">شرق ◀</text><text x="306" y="176" font-size="13" text-anchor="end" fill="var(--mute)">▶ غرب</text>` +
    `<line x1="160" y1="150" x2="160" y2="40" stroke="var(--line)" stroke-dasharray="4 4"/>` +
    `<circle id="gTarget" r="7" fill="var(--accent)" stroke="var(--card)" stroke-width="2"/>` +
    `<g id="gPanel"><rect x="70" y="140" width="180" height="10" rx="3" fill="var(--panel)"/><line x1="160" y1="140" x2="160" y2="95" stroke="var(--accent)" stroke-width="3"/><polygon points="160,86 154,98 166,98" fill="var(--accent)"/></g>` +
    `<circle cx="160" cy="150" r="9" fill="var(--ink)"/>`;
}
function moveGauge(angle, target) {
  const clamp = (a) => Math.max(limits.min - 8, Math.min(limits.max + 8, a));
  const g = $("gPanel"), t = $("gTarget");
  if (!g || !t) return;
  g.setAttribute("transform", `rotate(${clamp(angle)} 160 150)`);
  const [x, y] = pt(clamp(target), 118);
  t.setAttribute("cx", x); t.setAttribute("cy", y);
}
buildGauge();

// ---- status (every second) ----
function deviceDate(utc, tz) { return new Date((utc + tz * 60) * 1000).toISOString().replace("T", " ").slice(0, 19); }
async function poll() {
  try {
    const r = await api("GET", "/api/status");
    if (!r.ok) throw new Error("status");
    status = r.data;
    $("conn").textContent = "متصل"; $("conn").className = "pill";
    $("angle").textContent = fa(status.angleDeg) + "°";
    $("target").textContent = status.state === "Manual" ? "حالت دستی" : "هدف: " + fa(status.targetDeg) + "°";
    $("wind").textContent = fa(status.windKmh, 0);
    $("windNote").textContent = status.windStow ? "حالت ایمن" : "km/h";
    $("mode").textContent = status.mode === "manual" ? "دستی" : "اتو";
    $("state").textContent = status.state;
    $("stateName").textContent = status.moving ? "در حال حرکت" : "ایستاده";
    $("fault").textContent = status.fault ? "خطا" : "—";
    $("faultText").textContent = status.fault ? status.text : "بدون خطا";
    $("statusText").textContent = status.text;
    const sid = status.statusId;
    const badge = status.fault ? "red" : (status.state === "Wind" || status.state === "Manual" || sid === 9 || sid === 10 || sid === 11) ? "amber" : "ok";
    $("statusBadge").textContent = status.fault ? "خطا" : badge === "amber" ? "توجه" : "سالم";
    $("statusBadge").className = "badge " + badge;
    $("fw").textContent = status.fwVersion;
    $("devTime").textContent = status.clockValid ? deviceDate(status.utc, status.tzOffsetMin) : "تنظیم نشده";
    $("clockNow").textContent = $("devTime").textContent;
    $("staState").textContent = status.staConnected ? "متصل " + status.staIp : "—";
    $("warnDefault").classList.toggle("hidden", !status.passwordIsDefault);
    $("warnClock").classList.toggle("hidden", status.clockValid);
    $("warnCal").classList.toggle("hidden", status.calibrated);
    $("resetFault").classList.toggle("hidden", !status.fault || !loggedIn);
    moveGauge(status.angleDeg, status.targetDeg);
  } catch (e) {
    $("conn").textContent = "قطع"; $("conn").className = "pill off";
  }
}
setInterval(poll, 1000);

$("resetFault").onclick = async () => {
  if (!(await confirmDialog("خطا ریست شود؟ مطمئن شوید علت آن (مثلاً گیر کردن بازو) برطرف شده است."))) return;
  const r = await api("POST", "/api/fault/reset", {});
  toast(r.ok ? "خطا ریست شد" : errText(r), r.ok ? "ok" : "err"); poll();
};

// ---- manual ----
function setSeg() { $("modeAuto").classList.toggle("on", status && status.mode === "auto"); $("modeManual").classList.toggle("on", status && status.mode === "manual"); }
$("modeAuto").onclick = async () => { const r = await api("POST", "/api/mode", { mode: "auto" }); toast(r.ok ? "حالت اتوماتیک" : errText(r), r.ok ? "ok" : "err"); };
$("modeManual").onclick = async () => { const r = await api("POST", "/api/mode", { mode: "manual" }); toast(r.ok ? "حالت دستی" : errText(r), r.ok ? "ok" : "err"); };
setInterval(setSeg, 1000);
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
const GROUPS = [
  ["حرکت و محدوده", [["moveThresholdDeg", "آستانه شروع حرکت"], ["minDeg", "حداقل زاویه (شرق)"], ["maxDeg", "حداکثر زاویه (غرب)"], ["preDawnMinutes", "آماده‌سازی پیش از طلوع"]]],
  ["ایمنی حرکت", [["stallCheckMs", "زمان بررسی گیرکردن"], ["stopSettleMs", "مکث بعد از توقف"], ["minMoveGapMs", "فاصله بین دو حرکت"], ["maxMoveMs", "سقف زمان یک حرکت"]]],
  ["باد", [["windStowKmh", "آستانه باد شدید"], ["windStowDelayS", "تأخیر ورود به حالت باد"], ["windReturnKmh", "آستانه بازگشت از باد"], ["windReturnMin", "مدت آرامش برای بازگشت"], ["anemometerFactor", "ضریب بادسنج"]]],
  ["ماژول جریان", [["currentPresent", "ماژول جریان نصب است"], ["currentFactor", "ضریب ترانس جریان"], ["stallCurrentA", "جریان گیرکردن"], ["maxCurrentA", "حداکثر جریان مجاز"], ["zeroCurrentA", "جریان «صفر»"]]],
  ["دستگاه", [["supplyLowV", "هشدار ولتاژ تغذیه"], ["apPassword", "رمز هات‌اسپات (پس از ریستارت)"]]],
];
const ALL_KEYS = GROUPS.flatMap((g) => g[1].map((f) => f[0]));
function shown(k) { const v = settingsData[k]; return typeof v === "boolean" ? (v ? "1" : "0") : v === undefined ? "" : String(v); }
function renderSettings() {
  $("settingsForm").innerHTML = GROUPS.map(([title, fields], gi) =>
    `<details class="card group"${gi < 2 ? " open" : ""}><summary>${title}</summary>` + fields.map(([k, label]) => {
      const unit = HELP[k] && HELP[k].unit && k !== "currentPresent" ? ` <span class="small">(${HELP[k].unit})</span>` : "";
      const type = k === "apPassword" ? "password" : "text";
      return `<div class="field" id="f_${k}"><label for="i_${k}">${label}${unit}</label><input id="i_${k}" type="${type}" inputmode="${k === "apPassword" ? "text" : "decimal"}" autocomplete="off" value="${k === "apPassword" ? "" : shown(k)}"${k === "apPassword" ? ' placeholder="بدون تغییر"' : ""}><button class="help" type="button" data-help="${k}" aria-label="راهنما">؟</button></div><p class="helptext hidden" id="h_${k}"></p>`;
    }).join("") + `</details>`).join("");
  bindHelp($("settingsForm"));
  for (const k of ALL_KEYS) $("i_" + k).oninput = markDirty;
  markDirty();
}
function toNum(v) { return Number(v.trim().replace(/[٠-٩]/g, (d) => d.charCodeAt(0) - 1632).replace(/[۰-۹]/g, (d) => d.charCodeAt(0) - 1776).replace("٫", ".")); }
function changes() {
  const body = {};
  for (const k of ALL_KEYS) {
    const v = $("i_" + k).value.trim();
    if (k === "apPassword") { if (v) body[k] = v; continue; }
    if (v === "" || v === shown(k)) continue;
    if (k === "currentPresent") body[k] = v === "1"; else body[k] = toNum(v);
  }
  return body;
}
function markDirty() {
  const c = changes();
  for (const k of ALL_KEYS) $("f_" + k).classList.toggle("dirty", k in c);
  const n = Object.keys(c).length;
  $("dirtyBar").classList.toggle("hidden", n === 0);
  $("dirtyBar").textContent = `${fa(n, 0)} تغییر ذخیره‌نشده دارید.`;
  $("saveSettings").disabled = n === 0;
}
async function loadSettings() {
  const r = await api("GET", "/api/settings");
  if (!r.ok) return;
  settingsData = r.data;
  limits = { min: settingsData.minDeg, max: settingsData.maxDeg };
  buildGauge();
  renderSettings();
  $("locNow").textContent = `${settingsData.latDeg.toFixed(3)}, ${settingsData.lonDeg.toFixed(3)} (${settingsData.cityId})`;
  $("citySel").value = cities.some((c) => c.id === settingsData.cityId) ? settingsData.cityId : "";
}
$("saveSettings").onclick = async () => {
  document.querySelectorAll(".field.bad").forEach((e) => e.classList.remove("bad"));
  const r = await api("POST", "/api/settings", changes());
  if (r.ok) { toast("تنظیمات ذخیره شد", "ok"); loadSettings(); }
  else {
    toast(errText(r) + (r.data.field ? ": " + r.data.field : ""), "err");
    const f = $("f_" + r.data.field); if (f) { f.classList.add("bad"); f.scrollIntoView({ block: "center" }); const d = f.closest("details"); if (d) d.open = true; }
  }
};
$("revertSettings").onclick = () => renderSettings();
window.addEventListener("beforeunload", (e) => { if (loggedIn && $("saveSettings") && !$("saveSettings").disabled && Object.keys(changes()).length) { e.preventDefault(); e.returnValue = ""; } });

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
  toast(r.ok ? "موقعیت ذخیره شد" : errText(r), r.ok ? "ok" : "err"); if (r.ok) loadSettings();
};
$("saveCoord").onclick = async () => {
  const r = await api("POST", "/api/location", { text: $("coordText").value });
  toast(r.ok ? "موقعیت ذخیره شد" : errText(r), r.ok ? "ok" : "err"); if (r.ok) loadSettings();
};

// ---- clock ----
$("syncPhone").onclick = async () => {
  const r = await api("POST", "/api/clock", { utc: Math.floor(Date.now() / 1000) });
  toast(r.ok ? "ساعت با گوشی همگام شد" : errText(r), r.ok ? "ok" : "err"); poll();
};
$("saveClock").onclick = async () => {
  const v = $("clockManual").value;
  if (!v || !status) return;
  const p = v.match(/(\d+)-(\d+)-(\d+)T(\d+):(\d+):?(\d*)/);
  const local = Date.UTC(+p[1], +p[2] - 1, +p[3], +p[4], +p[5], +(p[6] || 0)) / 1000;
  const r = await api("POST", "/api/clock", { utc: local - status.tzOffsetMin * 60 });
  toast(r.ok ? "ساعت ثبت شد" : errText(r), r.ok ? "ok" : "err"); poll();
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
$("calSave").onclick = async () => { const r = await api("POST", "/api/calibrate/save", {}); toast(r.ok ? "کالیبراسیون ذخیره شد" : errText(r), r.ok ? "ok" : "err"); poll(); };

// ---- log ----
let logEvents = [], logFilter = "all", logInfo = null;
const MOVES = ["MoveStart", "MoveEnd"], FAULTS = ["Fault", "FaultReset"];
function renderLog() {
  const tz = status ? status.tzOffsetMin : 0;
  const keep = (e) => logFilter === "all" || (logFilter === "fault" && FAULTS.includes(e.type)) || (logFilter === "move" && MOVES.includes(e.type)) || (logFilter === "system" && !FAULTS.includes(e.type) && !MOVES.includes(e.type));
  $("logRows").innerHTML = logEvents.filter(keep).slice().reverse().map((e) =>
    `<tr class="${FAULTS.includes(e.type) ? "f" : ""}"><td class="t">${e.t > 0 ? deviceDate(e.t, tz) : "—"}</td><td>${e.type}</td><td>${String(e.detail).replace(/[<&]/g, "")}</td></tr>`).join("");
  if (logInfo) {
    const pct = Math.min(100, (logInfo.storedBytes / logInfo.maxBytes) * 100);
    $("logFill").style.width = pct + "%";
    $("logFill").className = pct > 85 ? "high" : "";
    $("logInfo").textContent = `حافظه لاگ: ${fa(logInfo.storedBytes / 1024, 0)} از ${fa(logInfo.maxBytes / 1024, 0)} کیلوبایت (حدود ${fa(logInfo.maxBytes / 55, 0)} رویداد). با پر شدن، قدیمی‌ترین نیمه خودکار پاک می‌شود. ${fa(logEvents.length, 0)} رویداد اخیر نمایش داده می‌شود.`;
  }
}
async function loadLog() {
  const r = await api("GET", "/api/log?n=100");
  if (!r.ok) return;
  logEvents = r.data.events; logInfo = r.data;
  renderLog();
}
$("logReload").onclick = loadLog;
$("logChips").onclick = (e) => {
  const f = e.target.dataset && e.target.dataset.f;
  if (!f) return;
  logFilter = f;
  document.querySelectorAll("#logChips button").forEach((b) => b.classList.toggle("on", b.dataset.f === f));
  renderLog();
};

// ---- site Wi-Fi and notifications ----
function bars(rssi) { return rssi > -55 ? "▂▄▆█" : rssi > -67 ? "▂▄▆" : rssi > -78 ? "▂▄" : "▂"; }
async function loadNetwork() {
  const r = await api("GET", "/api/network");
  if (!r.ok) return;
  $("netSsid").value = r.data.ssid;
  $("chatId").value = r.data.chatId;
  $("notifyOn").checked = r.data.enabled;
  $("netState").textContent = r.data.ssid ? (r.data.connected ? "متصل · " + r.data.ip : "متصل نیست") : "تنظیم نشده";
  $("botToken").placeholder = r.data.tokenSet ? "ثبت شده" : "";
  $("netPass").placeholder = r.data.passwordSet ? "ثبت شده" : "";
}
$("scanBtn").onclick = async () => {
  $("scanBtn").disabled = true;
  $("scanList").innerHTML = '<li>در حال جستجو…</li>';
  await api("POST", "/api/wifi/scan", {});
  for (let i = 0; i < 20; i++) {
    await new Promise((res) => setTimeout(res, 1000));
    const r = await api("GET", "/api/wifi/scan");
    if (r.ok && r.data.state === "done") {
      $("scanList").innerHTML = r.data.networks.length
        ? r.data.networks.map((n) => `<li data-ssid="${n.ssid.replace(/"/g, "&quot;")}"><span>${n.ssid.replace(/[<&]/g, "")}</span><span class="bars">${bars(n.rssi)} ${n.secure ? "🔒" : ""}</span></li>`).join("")
        : "<li>شبکه‌ای پیدا نشد</li>";
      break;
    }
  }
  $("scanBtn").disabled = false;
};
$("scanList").onclick = (e) => { const li = e.target.closest("li"); if (li && li.dataset.ssid) { $("netSsid").value = li.dataset.ssid; $("netPass").focus(); } };
$("netSave").onclick = async () => {
  const body = { ssid: $("netSsid").value };
  if ($("netPass").value) body.password = $("netPass").value;
  const r = await api("POST", "/api/network", body);
  toast(r.ok ? "ثبت شد؛ در حال اتصال" : errText(r), r.ok ? "ok" : "err");
  if (r.ok) { $("netPass").value = ""; loadNetwork(); }
};
$("notifySave").onclick = async () => {
  const body = { chatId: $("chatId").value, enabled: $("notifyOn").checked };
  if ($("botToken").value) body.token = $("botToken").value;
  const r = await api("POST", "/api/notify", body);
  toast(r.ok ? "ثبت شد" : errText(r), r.ok ? "ok" : "err");
  if (r.ok) { $("botToken").value = ""; loadNetwork(); }
};
$("notifyTest").onclick = async () => {
  const r = await api("POST", "/api/notify/test", {});
  toast(r.ok ? "پیام آزمایشی در صف ارسال است؛ در بله ببینید" : errText(r), r.ok ? "ok" : "err");
};

// ---- firmware update ----
$("fwSend").onclick = () => {
  const f = $("fwFile").files[0];
  if (!f) { say($("fwMsg"), "ابتدا فایل را انتخاب کنید", "err"); return; }
  const xhr = new XMLHttpRequest();
  const form = new FormData();
  form.append("firmware", f, f.name);
  xhr.open("POST", "/api/update");
  $("fwBar").classList.remove("hidden");
  xhr.upload.onprogress = (e) => { if (e.lengthComputable) { $("fwBar").value = (e.loaded / e.total) * 100; say($("fwMsg"), "در حال ارسال: " + fa((e.loaded / e.total) * 100, 0) + "٪"); } };
  xhr.onload = () => {
    let d = {};
    try { d = JSON.parse(xhr.responseText); } catch (e) { /* no body */ }
    if (xhr.status === 200) say($("fwMsg"), d.message || "نصب شد", "ok");
    else say($("fwMsg"), d.message || "ارسال ناموفق بود", "err");
  };
  xhr.onerror = () => say($("fwMsg"), "ارتباط قطع شد", "err");
  xhr.send(form);
};

// ---- password ----
$("pwSave").onclick = async () => {
  const r = await api("POST", "/api/password", { old: $("pwOld").value, new: $("pwNew").value });
  if (r.ok) { toast("رمز عوض شد؛ دوباره وارد شوید", "ok"); $("pwOld").value = ""; $("pwNew").value = ""; setLoggedIn(false); }
  else toast(errText(r), "err");
};

bindHelp(document);
async function loadAll() { await loadCities(); await loadSettings(); loadNetwork(); }

(async () => {
  await poll();
  const r = await api("GET", "/api/settings");  // 401 when there is no session yet
  setLoggedIn(r.ok);  // logging in loads everything
})();
