"use strict";
const $ = (id) => document.getElementById(id);
const fa = (n, d = 1) => Number(n).toLocaleString("fa-IR", { maximumFractionDigits: d, minimumFractionDigits: d });
// Digits: every number the user sees (also inside input boxes) is Persian; typed Persian, Arabic or Latin digits are all accepted.
const toFa = (s) => String(s).replace(/[0-9]/g, (d) => "۰۱۲۳۴۵۶۷۸۹"[d]).replace(/\./g, "٫");
const toLat = (s) => String(s).replace(/[٠-٩]/g, (d) => d.charCodeAt(0) - 1632).replace(/[۰-۹]/g, (d) => d.charCodeAt(0) - 1776).replace(/[٫]/g, ".").replace(/[،؛]/g, ",");
// A number with its unit as one left-to-right piece, so the sign and the degree mark do not jump around in right-to-left text.
const ltr = (txt) => `<bdi dir="ltr">${txt}</bdi>`;
// Panel convention: east is positive, west is negative. The firmware uses the opposite sign, so every angle is flipped here on the way in and out.
const uiAngle = (fw) => (fw === 0 ? 0 : -fw);
const FLIP = { minDeg: 1, maxDeg: 1 };  // settings whose shown value is the firmware value with the sign flipped
const degTxt = (n, d = 1) => ltr(fa(n, d) + "°");

// ---- help texts: one per field. min/max/def must match the validation in the firmware (a host test compares them) ----
const HELP = /*HELP-BEGIN*/{
  "moveThresholdDeg": {"t": "کمترین اختلاف بین زاویه پنل و زاویه‌ای که خورشید می‌خواهد تا دستگاه حرکت کند. عدد کم‌تر یعنی ردیابی دقیق‌تر ولی حرکت و قطع و وصل رله بیشتر؛ عدد بیشتر یعنی حرکت کمتر و خطای ردیابی بیشتر. با ۵ درجه فقط حدود ۰٫۴٪ تولید از دست می‌رود.", "min": 2, "max": 15, "def": 5, "unit": "درجه"},
  "minDeg": {"t": "انتهای شرقی بازه چرخش پنل (مثبت یعنی شرق، مثلاً ۴۵+). پنل هیچ‌وقت بیشتر از این به شرق نمی‌رود. باید با محدودیت مکانیکی سازه و جک هماهنگ باشد و از انتهای غرب بیشتر باشد.", "min": -60, "max": 0, "def": -45, "unit": "درجه"},
  "maxDeg": {"t": "انتهای غربی بازه چرخش پنل (منفی یعنی غرب، مثلاً ۴۵−). پنل هیچ‌وقت بیشتر از این به غرب نمی‌رود. باید با محدودیت مکانیکی سازه و جک هماهنگ باشد و از انتهای شرق کمتر باشد.", "min": 0, "max": 60, "def": 45, "unit": "درجه"},
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
  "gauge": {"t": "<b>نماد شرق و غرب:</b> خاکستری یعنی فرمانی نیست. نارنجی یعنی فرمان حرکت صادر شده و جهت انتخاب شده ولی موتور هنوز روشن نشده (تأخیر ایمنی). سبز یعنی موتور روشن است و پنل در همین جهت حرکت می‌کند.<br><b>عدد وسط:</b> اختلاف زاویه پنل با هدف، تقسیم بر آستانه شروع حرکت. وقتی اختلاف از آستانه بیشتر شود موتور شروع به حرکت می‌کند.<br><b>برچسب رنگی:</b> «بدون خطا» یعنی هیچ خطایی ثبت نشده و کنترل عادی کار می‌کند. «توجه» یعنی خطایی نیست ولی دستگاه در حالت ویژه است (دستی، باد شدید، انتظار تأخیر ایمنی، یا ساعت و کالیبراسیون تنظیم نشده). «خطا» یعنی دستگاه متوقف شده و باید علت را برطرف و خطا را ریست کنید."},
  "sunEl": {"t": "زاویهٔ خورشید بالای افق: ۰ درجه یعنی خورشید روی افق است (طلوع یا غروب) و ۹۰ درجه یعنی درست بالای سر. این زاویه در تابستان بالاتر و در زمستان پایین‌تر است. منفی یعنی خورشید زیر افق و شب است. دستگاه آن را از ساعت و موقعیت جغرافیایی حساب می‌کند."},
  "clock": {"t": "ساعت محلی را وارد کنید؛ دستگاه با منطقه زمانی تنظیم‌شده آن را به UTC تبدیل می‌کند. ساده‌تر این است که تیک «همگام‌سازی خودکار ساعت» را بزنید تا ساعت دستگاه با ساعت کامپیوتر یا گوشی که پنل را باز کرده (و با اینترنت، اگر وصل باشد) یکی شود. با برداشتن تیک، ساعت فقط دستی تغییر می‌کند."},
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
  if (key === "sunEl" && status && status.sunMaxElevationDeg !== null && status.sunMaxElevationDeg !== undefined) s += `<br><b>بالاترین زاویه امروز:</b> ${degTxt(status.sunMaxElevationDeg)} (نزدیک ظهر خورشیدی)`;
  if (h.min !== undefined && key !== "currentPresent") {
    const lo = FLIP[key] ? uiAngle(h.max) : h.min, hi = FLIP[key] ? uiAngle(h.min) : h.max, def = FLIP[key] ? uiAngle(h.def) : h.def;
    s += `<br><b>بازه مجاز:</b> ${ltr(fa(lo, lo % 1 ? 2 : 0))} تا ${ltr(fa(hi, hi % 1 ? 2 : 0))} ${h.unit || ""} · <b>پیش‌فرض:</b> ${ltr(fa(def, def % 1 ? 1 : 0))}`;
  }
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

// ---- tabs: four sections, the pages of the open section show in a second row ----
const NAV = [["وضعیت", [["dash", "وضعیت"]]], ["کنترل", [["manual", "دستی"], ["calib", "کالیبراسیون"]]],
  ["تنظیمات", [["settings", "پارامترها"], ["location", "موقعیت"], ["clock", "ساعت"], ["network", "شبکه"], ["account", "رمز"]]],
  ["ابزار", [["log", "لاگ"], ["update", "به‌روزرسانی"]]]];
function buildTabs(active) {
  const grp = NAV.findIndex(([, pages]) => pages.some(([id]) => id === active));
  let html = '<div class="navMain">' + NAV.map(([title, pages], i) => `<button data-tab="${pages[0][0]}"${i === grp ? ' class="on"' : ""}>${title}</button>`).join("") + "</div>";
  const pages = NAV[grp < 0 ? 0 : grp][1];
  if (pages.length > 1) html += '<div class="navSub">' + pages.map(([id, label]) => `<button data-tab="${id}"${id === active ? ' class="on"' : ""}>${label}</button>`).join("") + "</div>";
  $("tabs").innerHTML = html;
}
function showTab(name) {
  if (name !== "dash" && !loggedIn) name = "dash";
  document.querySelectorAll("main section").forEach((s) => s.classList.toggle("hidden", s.id !== name));
  buildTabs(name);
  if (name === "log") loadLog();
  if (name === "network") loadNetwork();
  if (name === "clock") fillClock();
}
$("tabs").onclick = (e) => { const t = e.target.dataset && e.target.dataset.tab; if (t) showTab(t); };
buildTabs("dash");

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
    `<rect x="0" y="0" width="320" height="200" rx="12" fill="var(--sky)"/>` +
    `<path d="M${x1} ${y1} A118 118 0 0 1 ${x2} ${y2}" fill="none" stroke="var(--line)" stroke-width="10" stroke-linecap="round"/>` +
    lab(limits.min, fa(limits.min, 0) + "°") + lab(0, "‎۰°") + lab(limits.max, "+" + fa(limits.max, 0) + "°") +
    `<rect id="gDiffBox" x="98" y="171" width="124" height="26" rx="13" fill="var(--card)" stroke="var(--line)"/><text id="gAngle" x="160" y="189" font-size="13" font-weight="700" text-anchor="middle" fill="var(--ink)" direction="ltr"></text>` +
    `<g id="gWest" class="dir"><rect x="10" y="171" width="76" height="26" rx="13"/><text x="48" y="189" text-anchor="middle" font-size="13">◀ غرب</text></g><g id="gEast" class="dir"><rect x="234" y="171" width="76" height="26" rx="13"/><text x="272" y="189" text-anchor="middle" font-size="13">شرق ▶</text></g>` +
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
// Jalali (Persian) calendar, Borkowski-style arithmetic conversion (valid for 1901-2099 Gregorian).
function g2j(gy, gm, gd) {
  const gdm = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334];
  const gy2 = gm > 2 ? gy + 1 : gy;
  let days = 355666 + 365 * gy + Math.floor((gy2 + 3) / 4) - Math.floor((gy2 + 99) / 100) + Math.floor((gy2 + 399) / 400) + gd + gdm[gm - 1];
  let jy = -1595 + 33 * Math.floor(days / 12053); days %= 12053;
  jy += 4 * Math.floor(days / 1461); days %= 1461;
  if (days > 365) { jy += Math.floor((days - 1) / 365); days = (days - 1) % 365; }
  const jm = days < 186 ? 1 + Math.floor(days / 31) : 7 + Math.floor((days - 186) / 30);
  const jd = 1 + (days < 186 ? days % 31 : (days - 186) % 30);
  return [jy, jm, jd];
}
function j2g(jy, jm, jd) {
  jy += 1595;
  let days = -355668 + 365 * jy + Math.floor(jy / 33) * 8 + Math.floor(((jy % 33) + 3) / 4) + jd + (jm < 7 ? (jm - 1) * 31 : (jm - 7) * 30 + 186);
  let gy = 400 * Math.floor(days / 146097); days %= 146097;
  if (days > 36524) { gy += 100 * Math.floor(--days / 36524); days %= 36524; if (days >= 365) days++; }
  gy += 4 * Math.floor(days / 1461); days %= 1461;
  if (days > 365) { gy += Math.floor((days - 1) / 365); days = (days - 1) % 365; }
  let gd = days + 1;
  const ml = [0, 31, (gy % 4 === 0 && gy % 100 !== 0) || gy % 400 === 0 ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  let gm = 0;
  while (gm < 13 && gd > ml[gm]) gd -= ml[gm++];
  return [gy, gm, gd];
}
const pad2 = (n) => String(n).padStart(2, "0");
const JMONTH = ["فروردین", "اردیبهشت", "خرداد", "تیر", "مرداد", "شهریور", "مهر", "آبان", "آذر", "دی", "بهمن", "اسفند"];
// device-local calendar parts for a UTC epoch and zone offset (minutes)
function localParts(utc, tz) {
  const t = new Date((utc + tz * 60) * 1000);
  return { y: t.getUTCFullYear(), m: t.getUTCMonth() + 1, d: t.getUTCDate(), hh: t.getUTCHours(), mm: t.getUTCMinutes(), ss: t.getUTCSeconds() };
}
function jalaliText(utc, tz) { const p = localParts(utc, tz), j = g2j(p.y, p.m, p.d); return toFa(`${j[0]}/${pad2(j[1])}/${pad2(j[2])}`); }
function timeText(utc, tz) { const p = localParts(utc, tz); return toFa(`${pad2(p.hh)}:${pad2(p.mm)}:${pad2(p.ss)}`); }
function deviceDate(utc, tz) { return new Date((utc + tz * 60) * 1000).toISOString().replace("T", " ").slice(0, 19); }
// Persian digits with "-" or a plain space between them are reordered by the bidi algorithm (the date shows reversed): use "/" and a left-to-right mark.
function showDate(utc, tz) { return jalaliText(utc, tz) + " ‎" + timeText(utc, tz); }
// Live clock: the device time from the last poll, advanced locally every second.
let clockBase = null;
function renderClock() {
  if (!clockBase) return;
  const utc = clockBase.utc + Math.floor((Date.now() - clockBase.at) / 1000), tz = clockBase.tz;
  const p = localParts(utc, tz), j = g2j(p.y, p.m, p.d);
  $("liveTime").textContent = timeText(utc, tz);
  $("liveDate").textContent = `${toFa(j[2])} ${JMONTH[j[1] - 1]} ${toFa(j[0])}`;
  $("devTime").textContent = showDate(utc, tz);
  $("clockNow").textContent = showDate(utc, tz);
}
setInterval(renderClock, 1000);
async function poll() {
  try {
    const r = await api("GET", "/api/status");
    if (!r.ok) throw new Error("status");
    status = r.data;
    $("conn").textContent = "متصل"; $("conn").className = "pill";
    $("wind").innerHTML = ltr(fa(status.windKmh, 1));
    $("windNote").textContent = status.windStow ? "حالت ایمن باد" : "کیلومتر بر ساعت";
    $("mode").textContent = status.mode === "manual" ? "دستی" : "اتو";
    $("state").textContent = status.state;
    $("stateName").textContent = status.moving ? "در حال حرکت" : "ایستاده";
    $("fault").textContent = status.fault ? "خطا" : "—";
    $("faultText").textContent = status.fault ? status.text : "بدون خطا";
    $("statusText").textContent = status.text;
    const sid = status.statusId;
    const badge = status.fault ? "red" : (status.state === "Wind" || status.state === "Manual" || sid === 9 || sid === 10 || sid === 11) ? "amber" : "ok";
    $("statusBadge").textContent = status.fault ? "خطا" : badge === "amber" ? "توجه" : "بدون خطا";
    $("statusBadge").className = "badge " + badge;
    $("fw").textContent = status.fwVersion;
    if (status.clockValid) { clockBase = { utc: status.utc, tz: status.tzOffsetMin, at: Date.now() }; renderClock(); }
    else { clockBase = null; $("devTime").textContent = $("clockNow").textContent = $("liveTime").textContent = "تنظیم نشده"; $("liveDate").textContent = ""; }
    const hasSun = status.sunElevationDeg !== null && status.sunElevationDeg !== undefined;
    $("sunEl").innerHTML = hasSun ? degTxt(status.sunElevationDeg) : "—";
    $("sunNote").textContent = !hasSun ? "" : status.sunElevationDeg < 0 ? "زیر افق (شب)" : "بالای افق";
    netFromStatus();
    $("staState").innerHTML = status.staConnected ? "متصل " + ltr(status.staIp) : "—";
    $("warnDefault").classList.toggle("hidden", !status.passwordIsDefault);
    $("warnClock").classList.toggle("hidden", status.clockValid);
    $("warnCal").classList.toggle("hidden", status.calibrated);
    if (document.activeElement !== $("clockAuto")) $("clockAuto").checked = !!status.clockAuto;
    autoClockSync();
    if (!status.calibrated && status.gravity) { const g = status.gravity.map((v) => v.toFixed(2)); $("rawG").textContent = "سنسور زنده: x=" + g[0] + " y=" + g[1] + " z=" + g[2]; }
    $("faultBar").classList.toggle("hidden", !status.fault);
    $("faultBarText").textContent = status.fault ? status.text : "";
    $("resetFault").classList.toggle("hidden", !status.fault || !loggedIn);
    moveGauge(uiAngle(status.angleDeg), uiAngle(status.targetDeg));
    // difference between the panel and where it should point; the motor starts when it reaches the move threshold
    const diff = uiAngle(status.targetDeg) - uiAngle(status.angleDeg), thr = status.moveThresholdDeg;
    $("gAngle").textContent = "‎Δ " + toFa(Math.abs(diff).toFixed(1)) + "° / " + toFa(Number(thr).toFixed(0)) + "°";
    $("gDiffBox").setAttribute("stroke", Math.abs(diff) > thr ? "var(--accent)" : "var(--line)");
    const dirClass = (d) => "dir" + (status.cmdDir === d ? (status.moving ? " run" : " cmd") : "");
    $("gEast").setAttribute("class", dirClass("east")); $("gWest").setAttribute("class", dirClass("west"));
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
  ["حرکت و محدوده", [["moveThresholdDeg", "آستانه شروع حرکت"], ["minDeg", "انتهای شرق (زاویه مثبت)"], ["maxDeg", "انتهای غرب (زاویه منفی)"], ["preDawnMinutes", "آماده‌سازی پیش از طلوع"]]],
  ["ایمنی حرکت", [["stallCheckMs", "زمان بررسی گیرکردن"], ["stopSettleMs", "مکث بعد از توقف"], ["minMoveGapMs", "فاصله بین دو حرکت"], ["maxMoveMs", "سقف زمان یک حرکت"]]],
  ["باد", [["windStowKmh", "آستانه باد شدید"], ["windStowDelayS", "تأخیر ورود به حالت باد"], ["windReturnKmh", "آستانه بازگشت از باد"], ["windReturnMin", "مدت آرامش برای بازگشت"], ["anemometerFactor", "ضریب بادسنج"]]],
  ["ماژول جریان", [["currentPresent", "ماژول جریان نصب است"], ["currentFactor", "ضریب ترانس جریان"], ["stallCurrentA", "جریان گیرکردن"], ["maxCurrentA", "حداکثر جریان مجاز"], ["zeroCurrentA", "جریان «صفر»"]]],
  ["دستگاه", [["supplyLowV", "هشدار ولتاژ تغذیه"]]],
];
const ALL_KEYS = GROUPS.flatMap((g) => g[1].map((f) => f[0]));
function shown(k) { const v = FLIP[k] ? uiAngle(settingsData[k]) : settingsData[k]; return toFa(typeof v === "boolean" ? (v ? "1" : "0") : v === undefined ? "" : String(v)); }
function renderSettings() {
  $("settingsForm").innerHTML = GROUPS.map(([title, fields], gi) =>
    `<details class="card group"${gi < 2 ? " open" : ""}><summary>${title}</summary>` + fields.map(([k, label]) => {
      const unit = HELP[k] && HELP[k].unit && k !== "currentPresent" ? ` <span class="small">(${HELP[k].unit})</span>` : "";
      return `<div class="field" id="f_${k}"><label for="i_${k}">${label}${unit}</label><button class="help" type="button" data-help="${k}" aria-label="راهنما">؟</button><input id="i_${k}" type="text" inputmode="decimal" autocomplete="off" value="${shown(k)}"></div><p class="helptext hidden" id="h_${k}"></p>`;
    }).join("") + `</details>`).join("");
  bindHelp($("settingsForm"));
  for (const k of ALL_KEYS) $("i_" + k).oninput = markDirty;
  markDirty();
}
function toNum(v) { return Number(toLat(v.trim())); }
function changes() {
  const body = {};
  for (const k of ALL_KEYS) {
    const v = $("i_" + k).value.trim();
    if (v === "" || toLat(v) === toLat(shown(k))) continue;
    if (k === "currentPresent") body[k] = toLat(v) === "1"; else body[k] = FLIP[k] ? uiAngle(toNum(v)) : toNum(v);
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
  limits = { min: uiAngle(settingsData.maxDeg), max: uiAngle(settingsData.minDeg) };
  buildGauge();
  renderSettings();
  $("locNow").innerHTML = ltr(toFa(`${settingsData.latDeg.toFixed(3)}, ${settingsData.lonDeg.toFixed(3)}`)) + (settingsData.cityId ? ` (${(cities.find((c) => c.id === settingsData.cityId) || {}).name || settingsData.cityId})` : "");
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
let locByCity = true;
function setLocTab(city) {
  locByCity = city;
  $("locCityTab").classList.toggle("on", city); $("locCoordTab").classList.toggle("on", !city);
  $("locCity").classList.toggle("hidden", !city); $("locCoord").classList.toggle("hidden", city);
}
$("locCityTab").onclick = () => setLocTab(true);
$("locCoordTab").onclick = () => setLocTab(false);
$("saveLoc").onclick = async () => {
  let body;
  if (locByCity) { if (!$("citySel").value) { toast("یک شهر انتخاب کنید", "err"); return; } body = { city: $("citySel").value }; }
  else body = { text: toLat($("coordText").value) };
  const r = await api("POST", "/api/location", body);
  toast(r.ok ? "موقعیت ذخیره شد" : errText(r), r.ok ? "ok" : "err"); if (r.ok) loadSettings();
};

// ---- clock ----
// After a clock write, check that it stuck: with the site Wi-Fi connected the internet time (NTP) overwrites it.
async function clockSetDone(r, wantUtc) {
  if (!r.ok) { toast(errText(r), "err"); poll(); return; }
  await new Promise((res) => setTimeout(res, 2500));
  await poll();
  if (status && status.clockValid && Math.abs(status.utc - (wantUtc + 2)) > 10) {
    toast("ثبت شد ولی ساعت دوباره عوض شد؛ همگام‌سازی خودکار روشن است", "err");
  } else toast("ساعت ثبت شد", "ok");
}
let autoSyncAt = 0;
// With the tick on, the open panel keeps the device clock equal to this computer's/phone's clock.
function autoClockSync() {
  if (!status || !status.clockAuto || Date.now() - autoSyncAt < 30000) return;
  const want = Math.floor(Date.now() / 1000);
  if (status.clockValid && Math.abs(status.utc - want) <= 3) return;
  autoSyncAt = Date.now();
  api("POST", "/api/clock", { utc: want });
}
$("clockAuto").onchange = async () => {
  const on = $("clockAuto").checked;
  const r = await api("POST", "/api/settings", { clockAuto: on });
  if (!r.ok) { toast(errText(r), "err"); $("clockAuto").checked = !on; return; }
  toast(on ? "همگام‌سازی خودکار روشن شد" : "همگام‌سازی خودکار خاموش شد؛ ساعت را دستی ثبت کنید", "ok");
  autoSyncAt = 0; await poll();
};
function fillClock() {
  if (!status || !status.clockValid || $("clockDate").value || $("clockTime").value) return;
  $("clockDate").value = jalaliText(status.utc, status.tzOffsetMin); $("clockTime").value = timeText(status.utc, status.tzOffsetMin);
}
$("saveClock").onclick = async () => {
  if (status && status.clockAuto) { toast("اول تیک «همگام‌سازی خودکار ساعت» را بردارید", "err"); return; }
  const d = toLat($("clockDate").value).trim().match(/^(\d{4})[-\/.](\d{1,2})[-\/.](\d{1,2})$/);
  const t = toLat($("clockTime").value).trim().match(/^(\d{1,2}):(\d{2})(?::(\d{2}))?$/);
  if (!d || !t || !status) { toast("تاریخ شمسی را مثل ۱۴۰۶/۰۴/۳۱ و ساعت را مثل ۱۴:۳۰:۰۰ بنویسید", "err"); return; }
  if (+d[1] < 1300 || +d[2] < 1 || +d[2] > 12 || +d[3] < 1 || +d[3] > 31) { toast("تاریخ شمسی معتبر نیست", "err"); return; }
  const g = j2g(+d[1], +d[2], +d[3]);
  const local = Date.UTC(g[0], g[1] - 1, g[2], +t[1], +t[2], +(t[3] || 0)) / 1000;
  const want = local - status.tzOffsetMin * 60;
  await clockSetDone(await api("POST", "/api/clock", { utc: want }), want);
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
const TYPE_FA = { PowerUp: "روشن شدن", StateChange: "تغییر وضعیت", Fault: "خطا", FaultReset: "ریست خطا", MoveStart: "شروع حرکت", MoveEnd: "پایان حرکت", ClockSet: "تنظیم ساعت",
  SettingsChange: "تغییر تنظیمات", SettingsReset: "بازنشانی تنظیمات", SettingsMigrated: "به‌روزرسانی ساختار تنظیمات", SupplyLow: "ولتاژ پایین", LogReset: "پاک شدن لاگ" };
const DETAIL_FA = [["no stored settings", "تنظیمات ذخیره‌شده‌ای نبود؛ پیش‌فرض‌ها بارگذاری شد"], ["power-on after cut", "روشن شدن پس از قطع برق"], ["power-on", "روشن شدن دستگاه"], ["wind stow start", "شروع حالت ایمن باد"], ["wind stow end", "پایان حالت ایمن باد"],
  ["end of stroke: limit reached", "انتهای کورس جک: به حد رسید"], ["manual keep-alive lost", "ارتباط دکمه دستی قطع شد"], ["angle calibration saved", "کالیبراسیون زاویه ذخیره شد"],
  ["notification settings changed", "تنظیمات هشدار عوض شد"], ["settings password changed", "رمز عوض شد"], ["site wifi changed", "وای‌فای محل عوض شد"], ["older layout", "ساختار قدیمی"],
  ["dir=east", "جهت: شرق"], ["dir=west", "جهت: غرب"], ["reason=", "علت: "], ["angle=", "زاویه: "], ["ms=", "مدت (میلی‌ثانیه): "], ["->", " ⟵ "],
  ["Auto", "خودکار"], ["Manual", "دستی"], ["Wind", "باد"], ["Night", "شب"], ["Fault", "خطا"], ["stop", "توقف"], ["target", "رسیدن به هدف"]];
function detailFa(d) { let t = String(d).replace(/[<&]/g, "").replace(/angle=(-?\d+(?:\.\d+)?)/g, (m, v) => "angle=" + uiAngle(parseFloat(v)).toFixed(1)); for (const [a, b] of DETAIL_FA) t = t.split(a).join(b); return toFa(t); }
function renderLog() {
  const tz = status ? status.tzOffsetMin : 0;
  const keep = (e) => logFilter === "all" || (logFilter === "fault" && FAULTS.includes(e.type)) || (logFilter === "move" && MOVES.includes(e.type)) || (logFilter === "system" && !FAULTS.includes(e.type) && !MOVES.includes(e.type));
  $("logRows").innerHTML = logEvents.filter(keep).slice().reverse().map((e) =>
    `<tr class="${FAULTS.includes(e.type) ? "f" : ""}"><td class="t">${e.t > 0 ? ltr(showDate(e.t, tz)) : "—"}</td><td class="k">${TYPE_FA[e.type] || e.type}</td><td>${detailFa(e.detail)}</td></tr>`).join("");
  $("log").classList.toggle("notime", !logEvents.some((e) => e.t > 0));  // no clock yet: the time column would only show dashes
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
const MASK = "••••••••";
function setMasked(el, isSet) {
  if (document.activeElement === el || (el.value && el.value !== MASK)) return;
  el.value = isSet ? MASK : "";
  el.dataset.masked = isSet ? "1" : "";
}
function secretValue(el) { return el.value && el.value !== MASK ? el.value : ""; }
for (const id of ["botToken", "netPass"]) $(id).addEventListener("focus", (e) => { if (e.target.value === MASK) e.target.select(); });
let netCfgSsid = "", netWasConnected = null;
// Green = connected (with signal bars), amber = set but not connected, grey = not set. Updated by every status poll.
function renderNetState(connected, ip, rssi) {
  const box = $("netBox");
  box.className = "netbox " + (connected ? "on" : netCfgSsid ? "wait" : "off");
  $("netState").innerHTML = connected ? "متصل به " + (netCfgSsid ? `<bdi dir="ltr">${netCfgSsid.replace(/[<&]/g, "")}</bdi> · ` : "") + ltr(ip) : netCfgSsid ? "متصل نیست؛ در حال تلاش…" : "تنظیم نشده";
  $("netBars").textContent = connected && rssi ? bars(rssi) + " " + fa(rssi, 0) + "dBm" : "";
}
function netFromStatus() {
  if (!status) return;
  const c = !!status.staConnected;
  if (netWasConnected !== null && c !== netWasConnected) toast(c ? "به وای‌فای محل وصل شد" : "اتصال وای‌فای محل قطع شد", c ? "ok" : "err");
  if (c !== netWasConnected && !$("network").classList.contains("hidden")) loadNetwork();
  netWasConnected = c;
  renderNetState(c, status.staIp, status.staRssi);
}
async function loadNetwork() {
  const r = await api("GET", "/api/network");
  if (!r.ok) return;
  if (!$("netSsid").value) $("netSsid").value = r.data.ssid;
  if (document.activeElement !== $("chatId")) $("chatId").value = r.data.chatId;  // never overwrite what is being typed
  if (document.activeElement !== $("notifyOn")) $("notifyOn").checked = r.data.enabled;
  netCfgSsid = r.data.ssid;
  if (!r.data.ssid) $("netChange").open = true;  // nothing set yet: show the form
  renderNetState(r.data.connected, r.data.ip, r.data.rssi);
  // A saved secret is shown as bullets (never the real value) so the field does not look empty.
  setMasked($("botToken"), r.data.tokenSet);
  setMasked($("netPass"), r.data.passwordSet);
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
  if (secretValue($("netPass"))) body.password = secretValue($("netPass"));
  const r = await api("POST", "/api/network", body);
  toast(r.ok ? "ثبت شد؛ در حال اتصال" : errText(r), r.ok ? "ok" : "err");
  if (r.ok) { $("netPass").value = ""; $("netPass").dataset.masked = ""; netCfgSsid = $("netSsid").value; $("netChange").open = false; renderNetState(false, "", 0); loadNetwork(); }
};
$("notifySave").onclick = async () => {
  const body = { chatId: $("chatId").value, enabled: $("notifyOn").checked };
  if (secretValue($("botToken"))) body.token = secretValue($("botToken"));
  const r = await api("POST", "/api/notify", body);
  toast(r.ok ? "ثبت شد" : errText(r), r.ok ? "ok" : "err");
  if (r.ok) { $("botToken").value = ""; loadNetwork(); }
};
$("notifyTest").onclick = async () => {
  const r = await api("POST", "/api/notify/test", {});
  toast(r.ok ? "پیام آزمایشی در صف ارسال است؛ در بله ببینید" : errText(r), r.ok ? "ok" : "err");
};

// ---- firmware update ----
$("fwPick").onclick = () => $("fwFile").click();
$("fwFile").onchange = () => { const f = $("fwFile").files[0]; $("fwName").textContent = f ? f.name : "فایلی انتخاب نشده"; };
$("fwSend").onclick = async () => {
  const f = $("fwFile").files[0];
  if (!f) { say($("fwMsg"), "ابتدا فایل را انتخاب کنید", "err"); return; }
  if (!(await confirmDialog("نرم‌افزار دستگاه با این فایل عوض می‌شود و دستگاه دوباره راه‌اندازی می‌شود. مطمئنید؟"))) return;
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
$("apSave").onclick = async () => {
  const v = $("apNew").value;
  if (v.length < 8 || v.length > 32) { toast("رمز وای‌فای باید ۸ تا ۳۲ نویسه باشد", "err"); return; }
  const r = await api("POST", "/api/settings", { apPassword: v });
  if (r.ok) { toast("رمز وای‌فای ذخیره شد؛ بعد از راه‌اندازی دوبارهٔ دستگاه اعمال می‌شود", "ok"); $("apNew").value = ""; }
  else toast(errText(r), "err");
};
$("pwSave").onclick = async () => {
  const r = await api("POST", "/api/password", { old: $("pwOld").value, new: $("pwNew").value });
  if (r.ok) { toast("رمز عوض شد؛ دوباره وارد شوید", "ok"); $("pwOld").value = ""; $("pwNew").value = ""; setLoggedIn(false); }
  else toast(errText(r), "err");
};

document.querySelectorAll("input[data-show]").forEach((c) => {
  c.onchange = () => c.dataset.show.split(",").forEach((id) => { $(id).type = c.checked ? "text" : "password"; });
});
bindHelp(document);
async function loadAll() { await loadCities(); await loadSettings(); loadNetwork(); }

(async () => {
  await poll();
  const r = await api("GET", "/api/settings");  // 401 when there is no session yet
  setLoggedIn(r.ok);  // logging in loads everything
})();
