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
  "windStowKmh": {"t": "اگر سرعت باد از این عدد بیشتر شود و برای «تأخیر ورود» بماند، پنل افقی می‌شود تا باد آسیب نزند. عدد کمتر یعنی محافظت بیشتر ولی توقف ردیابی در بادهای ملایم‌تر.", "min": 10, "max": 100, "def": 40, "unit": "کیلومتر بر ساعت"},
  "windStowDelayS": {"t": "باد باید این‌قدر ثانیه پشت سر هم بالاتر از آستانه بماند تا پنل افقی شود؛ تندبادهای کوتاه نادیده گرفته می‌شوند.", "min": 1, "max": 60, "def": 5, "unit": "ثانیه"},
  "windReturnKmh": {"t": "بعد از باد شدید، فقط وقتی باد زیر این عدد باشد شمارش بازگشت به ردیابی شروع می‌شود. باید از آستانه باد شدید کمتر باشد.", "min": 5, "max": 99, "def": 30, "unit": "کیلومتر بر ساعت"},
  "windReturnMin": {"t": "باد باید این‌قدر دقیقه پیوسته زیر «آستانه بازگشت» بماند تا ردیابی دوباره شروع شود. هر تندباد بالاتر از آستانه بازگشت، شمارش را از اول می‌کند.", "min": 1, "max": 120, "def": 15, "unit": "دقیقه"},
  "anemometerFactor": {"t": "ضریب تبدیل دور بادسنج به سرعت: چند کیلومتر بر ساعت به ازای هر دور در ثانیه. برای بادسنج ۲۰ سانتی حدود ۶٫۸ است. برای دقت بیشتر در محل نصب با یک بادسنج دستی مقایسه و تنظیم کنید.", "min": 1, "max": 20, "def": 6.8, "unit": "کیلومتر بر ساعت به ازای هر دور در ثانیه"},
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
  if (h.min !== undefined && key !== "currentPresent") {
    const lo = FLIP[key] ? uiAngle(h.max) : h.min, hi = FLIP[key] ? uiAngle(h.min) : h.max, def = FLIP[key] ? uiAngle(h.def) : h.def;
    s += `<br><b>بازه مجاز:</b> ${ltr(fa(lo, lo % 1 ? 2 : 0))} تا ${ltr(fa(hi, hi % 1 ? 2 : 0))} ${h.unit || ""} · <b>پیش‌فرض:</b> ${ltr(fa(def, def % 1 ? 1 : 0))}`;
  }
  else if (h.unit) s += `<br><b>پیش‌فرض:</b> ${fa(h.def, 0)} (${h.unit})`;
  return s;
}
function bindHelp(root) {
  root.querySelectorAll("button.help, .hl[data-help]").forEach((b) => {  // the "؟" buttons of the forms, and (dashboard) the topic itself
    const open = (e) => {
      e.preventDefault();
      const box = $("h_" + b.dataset.help);
      if (!box) return;
      box.innerHTML = helpHtml(b.dataset.help);
      box.classList.toggle("hidden");
    };
    b.onclick = open;
    if (b.getAttribute("role") === "button") b.onkeydown = (e) => { if (e.key === "Enter" || e.key === " ") open(e); };
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
const NAV = [["داشبورد", [["dash", "داشبورد"]]], ["کنترل", [["manual", "دستی"], ["calib", "کالیبراسیون"]]],
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

// ---- battery and mains in the header (cycle 026) ----
function renderPower(st) {
  const hasBat = st.batteryV != null, hasMains = st.mainsOk != null;
  $("pwr").classList.toggle("hidden", !hasBat && !hasMains);
  $("batt").classList.toggle("hidden", !hasBat);
  if (hasBat) {
    const pct = Math.max(0, Math.min(100, st.batteryPct));
    $("battFill").style.width = pct + "%";
    $("battPct").textContent = toFa(pct);
    $("batt").className = "batt" + (st.batteryState === "critical" ? " crit" : st.batteryState === "low" ? " low" : "");
    $("batt").title = "باتری: " + toFa(st.batteryV.toFixed(2)) + " ولت، حدود " + toFa(pct) + " درصد" + (st.batteryState === "low" ? " (کم)" : st.batteryState === "critical" ? " (تقریباً خالی)" : "");
  }
  $("mainsBox").classList.toggle("hidden", !hasMains);
  if (hasMains) {
    $("mainsBox").className = "mains" + (st.mainsOk ? "" : " off");
    $("mainsV").textContent = st.mainsOk ? toFa(st.mainsV.toFixed(1)) + " ولت" : "قطع";
    $("mainsBox").title = st.mainsOk ? "برق شهر وصل است؛ ولتاژ ورودی شارژر " + toFa(st.mainsV.toFixed(1)) + " ولت" : "برق شهر قطع است؛ دستگاه روی باتری کار می‌کند";
  }
}

// ---- tap an error, a warning or the wind row to read what it means (cycle 030) ----
const XP = {
  "fault:none": "خطایی ثبت نشده است. اگر خطایی رخ بدهد، علت و راه‌حل آن با زدن روی همین‌جا توضیح داده می‌شود.",
  "fault:stall": "گیر کردن بازو: بعد از فرمان حرکت، زاویه تغییر نکرد. علت‌های محتمل: بازو یا جک گیر کرده، سیم جک یا رله مشکل دارد، یا جهت رله برعکس بسته شده است. مسیر حرکت و سیم‌ها را ببینید و بعد «ریست خطا» را بزنید.",
  "fault:move-timeout": "زمان حرکت تمام شد: حرکت بیش از حد مجاز طول کشید و دستگاه آن را قطع کرد. علت محتمل: جک کند شده، به انتهای کورس نرسیده یا زاویه‌سنج درست نمی‌خواند. بعد از بررسی «ریست خطا» را بزنید.",
  "fault:angle-sensor": "سنسور زاویه جواب نمی‌دهد یا عدد نادرست می‌دهد. حرکت خودکار متوقف است. سیم‌های I2C و برد سنسور را ببینید. تا برطرف نشود دستگاه حرکت نمی‌کند.",
  "fault:interlock": "قفل ایمنی فعال شد: چیزی می‌خواست جهت را وقتی برق جک وصل بود عوض کند. این یک خطای نرم‌افزاری یا رله است. دستگاه را ریست کنید؛ اگر تکرار شد، رله‌ها را بررسی کنید.",
  "fault:reset-loop": "دستگاه چند بار پشت سر هم خودش را ریست کرده است (watchdog). علت محتمل: افت برق، سیم‌کشی ناپایدار یا نرم‌افزار. برق و سیم‌ها را بررسی کنید.",
  "fault:wire-break": "سیم جک قطع است: رله وصل شد ولی جریانی دیده نشد. سیم‌های جک و ترمینال‌ها را ببینید.",
  "fault:relay-stuck": "رله چسبیده: بعد از دستور توقف هنوز جریان می‌رود. برق جک را از کلید دستگاه قطع کنید و رله را بررسی یا عوض کنید.",
  "fault:overcurrent": "جریان جک بیش از حد مجاز شد. علت محتمل: بار سنگین، گیرکردن یا خرابی جک. بعد از بررسی «ریست خطا» را بزنید.",
  "warn:default": "رمز ورود پنل هنوز مقدار پیش‌فرض است و هر کسی که رمز را بداند می‌تواند دستگاه را کنترل کند. از بخش تنظیمات، برگه «رمز» آن را عوض کنید.",
  "warn:clock": "ساعت تنظیم نشده است و بدون ساعت، خورشید و ردیابی خودکار کار نمی‌کند. از بخش تنظیمات، برگه «ساعت» آن را تنظیم کنید یا همگام‌سازی خودکار را روشن کنید.",
  "warn:reversed": "دستگاه برعکس فرمان حرکت کرد. یا سیم‌های رله جهت جابه‌جا بسته شده‌اند یا در کالیبراسیون «شرق» به سمت اشتباه گرفته شده است. جلوی خطر، حرکت خودکار نمی‌کند.",
  "warn:angle": "زاویه دستگاه بیرون از محدودهٔ عادی است، مثلاً دستگاه برعکس یا کج نصب شده است. حرکت خودکار متوقف است ولی این خرابی سنسور نیست. دستگاه را در وضعیت نصب بگذارید.",
  "warn:cal": "کالیبراسیون زاویه انجام نشده است. بدون آن دستگاه زاویهٔ پنل را نمی‌داند و حرکت خودکار نمی‌کند. از بخش کنترل، برگه «کالیبراسیون» دو مرحله را انجام دهید.",
  "wind:normal": "سرعت باد از شمارندهٔ بادسنج حساب می‌شود. اگر بیش از آستانه (پیش‌فرض ۴۰ کیلومتر بر ساعت) چند ثانیه بماند، پنل برای ایمنی افقی می‌شود و بعد از ۱۵ دقیقه باد آرام برمی‌گردد.",
  "wind:stow": "حالت ایمن باد: باد شدید بوده و پنل افقی شده است. وقتی باد ۱۵ دقیقه پیوسته زیر آستانهٔ بازگشت بماند، ردیابی دوباره شروع می‌شود. دکمهٔ «ریست» بالای صفحه این حالت را زودتر تمام می‌کند.",
  "wind:sensor": "بادسنج عدد غیرممکن (بیش از ۱۲۰ کیلومتر بر ساعت) یا نوسان شدید می‌دهد. احتمالاً سیم بادسنج نویز می‌گیرد یا سنسور خراب است. تا آن موقع پنل به‌خاطر این عدد افقی نمی‌شود.",
};
function explainKey(el) {
  const k = el.dataset.x;
  if (k === "fault") return "fault:" + (status && status.fault ? status.fault : "none");
  if (k === "wind") return status && status.windSensorFault ? "wind:sensor" : status && status.windStow ? "wind:stow" : "wind:normal";
  return k;
}
function bindExplain() {
  document.querySelectorAll(".xp").forEach((el) => {
    el.classList.add("hl");
    const open = (e) => {
      if (e.target.closest("button")) return;  // the reset button keeps its own job
      const text = XP[explainKey(el)];
      if (!text) return;
      const at = el.closest(".grid, .faultBar, .li") || el;
      let box = at.nextElementSibling && at.nextElementSibling.classList.contains("xbox") ? at.nextElementSibling : null;
      if (!box) { box = document.createElement("p"); box.className = "helptext xbox hidden"; at.after(box); }
      box.textContent = text;
      box.classList.toggle("hidden");
    };
    el.addEventListener("click", open);
    if (el.getAttribute("role") === "button") el.addEventListener("keydown", (e) => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); open(e); } });
  });
}
bindExplain();
// tap the mode row to switch between automatic and manual
async function toggleMode() {
  if (!loggedIn) { toast("برای تغییر حالت ابتدا وارد شوید", "err"); return; }
  const to = status && status.mode === "manual" ? "auto" : "manual";
  const r = await api("POST", "/api/mode", { mode: to });
  toast(r.ok ? (to === "auto" ? "حالت اتوماتیک" : "حالت دستی") : errText(r), r.ok ? "ok" : "err");
  poll();
}
$("rowMode").onclick = toggleMode;
$("rowMode").onkeydown = (e) => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); toggleMode(); } };

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
const SC = { orbit: 120, range: 96 };  // radius of the sun's path and of the panel's working arc around the hub (160, 150)
const SCENE = "#dbe6f0";                // the sky is dark in both themes, so its labels are fixed
function buildGauge() {
  const [x1, y1] = pt(limits.min, SC.range), [x2, y2] = pt(limits.max, SC.range);
  const [o1x, o1y] = pt(-90, SC.orbit), [o2x, o2y] = pt(90, SC.orbit);
  const lab = (a, txt) => { const [x, y] = pt(a, SC.range - 16); return `<text x="${x}" y="${y}" text-anchor="middle" font-size="11" fill="${SCENE}" fill-opacity=".8">${txt}</text>`; };
  // Night scene (cycle 027): a crescent moon in the upper east corner (right) and a few stars around it, away from every label.
  const star = (x, y, r, d) => `<g class="tw" style="--d:${d}s"><circle cx="${x}" cy="${y}" r="${r}" fill="#fff"/></g>`;
  const night = `<g id="gNight"><defs><radialGradient id="gMoonGlow"><stop offset="0" stop-color="#cfe0ff" stop-opacity=".5"/><stop offset="1" stop-color="#cfe0ff" stop-opacity="0"/></radialGradient></defs><g class="stars">${star(228, 34, 1.5, 0)}${star(288, 30, 1.2, 1.1)}${star(284, 84, 1.4, 2.2)}${star(236, 76, 1, 0.6)}</g>` +
    `<g class="moon"><g transform="translate(262 52) rotate(-28)"><circle class="halo" r="28" fill="url(#gMoonGlow)"/><path d="M0 -13A13 13 0 1 0 0 13A16.5 16.5 0 0 1 0 -13Z" fill="#f4f1de"/></g></g></g>`;
  const rays = Array.from({ length: 8 }, (_, i) => { const a = (i * Math.PI) / 4; return `<line x1="${(17 * Math.cos(a)).toFixed(1)}" y1="${(17 * Math.sin(a)).toFixed(1)}" x2="${(23 * Math.cos(a)).toFixed(1)}" y2="${(23 * Math.sin(a)).toFixed(1)}"/>`; }).join("");
  $("gauge").innerHTML =
    `<rect id="gSky" x="0" y="0" width="320" height="200" rx="12" fill="#2a3f66"/>` +
    `<rect id="gBand" x="0" y="116" width="320" height="34" fill="#d9825b" opacity="0"/>` +
    `<path d="M${o1x} ${o1y} A${SC.orbit} ${SC.orbit} 0 0 1 ${o2x} ${o2y}" fill="none" stroke="#fff" stroke-opacity=".25" stroke-width="2" stroke-dasharray="2 6" stroke-linecap="round"/>` +
    night +
    `<path d="M${x1} ${y1} A${SC.range} ${SC.range} 0 0 1 ${x2} ${y2}" fill="none" stroke="#fff" stroke-opacity=".22" stroke-width="8" stroke-linecap="round"/>` +
    lab(limits.min, fa(limits.min, 0) + "°") + lab(limits.max, "+" + fa(limits.max, 0) + "°") +
    `<g id="gSun" style="display:none"><circle id="gGlow" r="26" opacity=".22"/><g id="gRays" stroke-width="2.5" stroke-linecap="round">${rays}</g><circle id="gDisc" r="12"/></g>` +
    `<path d="M0 150H320V188a12 12 0 0 1-12 12H12a12 12 0 0 1-12-12Z" fill="#070a10" fill-opacity=".9"/><line x1="0" y1="150" x2="320" y2="150" stroke="#fff" stroke-opacity=".35" stroke-width="1.5"/>` +
    `<text id="gUnder" x="160" y="24" font-size="12" text-anchor="middle" fill="#9fb0c6"></text>` +
    `<rect id="gDiffBox" x="98" y="171" width="124" height="26" rx="13" fill="var(--card)" stroke="var(--line)"/><text id="gAngle" x="160" y="189" font-size="13" font-weight="700" text-anchor="middle" fill="var(--ink)" direction="ltr"></text>` +
    `<g id="gWest" class="dir"><rect x="10" y="171" width="76" height="26" rx="13"/><text x="48" y="189" text-anchor="middle" font-size="13">◀ غرب</text></g><g id="gEast" class="dir"><rect x="234" y="171" width="76" height="26" rx="13"/><text x="272" y="189" text-anchor="middle" font-size="13">شرق ▶</text></g>` +
    `<line x1="160" y1="150" x2="160" y2="40" stroke="#fff" stroke-opacity=".25" stroke-dasharray="4 4"/>` +
    `<g id="gPanel"><rect x="70" y="140" width="180" height="10" rx="3" fill="#7fb0e0"/><line x1="160" y1="140" x2="160" y2="95" stroke="var(--accent)" stroke-width="3"/><polygon points="160,86 154,98 166,98" fill="var(--accent)"/></g>` +
    `<circle cx="160" cy="150" r="9" fill="#e8edf2"/>`;
}
function moveGauge(angle) {
  const clamp = (a) => Math.max(limits.min - 8, Math.min(limits.max + 8, a));
  const g = $("gPanel");
  if (g) g.setAttribute("transform", `rotate(${clamp(angle)} 160 150)`);
}
// ---- the sun: its place comes only from the clock and the location, never from faults or the panel ----
const lerp = (a, b, t) => a + (b - a) * t;
const hex3 = (c) => [1, 3, 5].map((i) => parseInt(c.slice(i, i + 2), 16));
const mixHex = (c1, c2, t) => { const a = hex3(c1), b = hex3(c2); return "#" + a.map((v, i) => Math.round(lerp(v, b[i], t)).toString(16).padStart(2, "0")).join(""); };
const ramp = (el, stops) => {
  if (el <= stops[0][0]) return stops[0][1];
  for (let i = 1; i < stops.length; i++) if (el <= stops[i][0]) return mixHex(stops[i - 1][1], stops[i][1], (el - stops[i - 1][0]) / (stops[i][0] - stops[i - 1][0]));
  return stops[stops.length - 1][1];
};
const SKY = [[-12, "#0d1424"], [-6, "#1b2748"], [0, "#2a3f66"], [10, "#3a63a0"], [30, "#2f6fb3"]];
const SUNC = [[0, "#ff8a4c"], [12, "#ffb347"], [30, "#ffc83d"]];
let sunPrev = null;
function moveSun(el, az) {
  const sun = $("gSun");
  if (!sun) return;
  if (el === null || el === undefined || az === null || az === undefined) { sun.style.display = "none"; $("gUnder").textContent = ""; return; }  // no clock: no sun
  const rad = Math.PI / 180;
  const th = Math.atan2(Math.cos(el * rad) * Math.sin(az * rad), Math.sin(el * rad)) / rad;  // + = east = right
  const [x, y] = pt(th, SC.orbit);
  const jump = sunPrev === null || Math.abs(th - sunPrev) > 90;
  sunPrev = th;
  sun.style.display = "";
  sun.style.transition = jump ? "none" : "transform 1s linear";
  sun.style.transform = `translate(${x.toFixed(1)}px, ${y.toFixed(1)}px)`;
  $("gSky").setAttribute("fill", ramp(el, SKY));
  $("gBand").setAttribute("opacity", (Math.max(0, 1 - Math.abs(el - 2) / 10) * 0.6).toFixed(2));
  $("gauge").classList.toggle("night", el < -4);  // the moon rises and the stars come out only at night
  const day = el >= 0, c = day ? ramp(el, SUNC) : "#8ea0b8";
  $("gGlow").setAttribute("fill", c); $("gGlow").style.display = day ? "" : "none";
  $("gRays").setAttribute("stroke", c); $("gRays").style.display = day ? "" : "none";
  const d = $("gDisc");
  d.setAttribute("fill", day ? c : "none");
  d.setAttribute("stroke", day ? "none" : c);
  d.setAttribute("stroke-width", "2"); d.setAttribute("stroke-dasharray", day ? "" : "4 4");
  $("gUnder").textContent = day ? "" : "خورشید زیر افق، \u2066" + toFa(Math.abs(el).toFixed(1)) + "°\u2069";
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
// Digital clock drawn with seven segments (no font, so every digit has the same width and nothing shifts when the seconds change).
const SEGS = ["abcdef", "bc", "abdeg", "abcdg", "bcfg", "acdfg", "acdefg", "abc", "abcdefg", "abcdfg"];
function lcdDigit(x, w, h, t, g) {
  const m = (h - t) / 2, vh = m - t - 2 * g;
  const r = { a: [t, 0, w - 2 * t, t], g: [t, m, w - 2 * t, t], d: [t, h - t, w - 2 * t, t], f: [0, t + g, t, vh], b: [w - t, t + g, t, vh], e: [0, m + t + g, t, vh], c: [w - t, m + t + g, t, vh] };
  return `<g class="dg" transform="translate(${x} ${h === 40 ? 0 : 16})">` + Object.entries(r).map(([k, v]) => `<rect data-s="${k}" x="${v[0]}" y="${v[1]}" width="${v[2]}" height="${v[3]}" rx="${t / 2.4}"/>`).join("") + "</g>";
}
function buildLcd() {
  const big = [0, 28, 66, 94], small = [122, 139];
  const dots = (x, y) => `<rect class="on" x="${x}" y="${y}" width="5" height="5" rx="1.5"/>`;
  $("liveTime").innerHTML = `<svg viewBox="0 0 152 40" aria-hidden="true">` + big.map((x) => lcdDigit(x, 22, 40, 5, 1.2)).join("") + dots(55, 12) + dots(55, 25) + small.map((x) => lcdDigit(x, 13, 24, 3, 0.8)).join("") + `</svg>`;
}
function setLcd(hh, mm, ss) {
  const d = [hh, mm, ss].map((v) => String(v).padStart(2, "0")).join("");
  $("liveOff").classList.add("hidden"); $("liveTime").classList.remove("hidden");
  if (!$("liveTime").firstChild) buildLcd();
  $("liveTime").setAttribute("aria-label", `${pad2(hh)}:${pad2(mm)}:${pad2(ss)}`);
  $("liveTime").querySelectorAll(".dg").forEach((g, i) => { const on = SEGS[+d[i]]; g.querySelectorAll("rect").forEach((r) => r.classList.toggle("on", on.includes(r.dataset.s))); });
}
// Live clock: the device time from the last poll, advanced locally every second.
let clockBase = null;
function renderClock() {
  if (!clockBase) return;
  const utc = clockBase.utc + Math.floor((Date.now() - clockBase.at) / 1000), tz = clockBase.tz;
  const p = localParts(utc, tz), j = g2j(p.y, p.m, p.d);
  setLcd(p.hh, p.mm, p.ss);
  $("liveDate").textContent = `${toFa(j[2])} ${JMONTH[j[1] - 1]} ${toFa(j[0])}`;
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
    renderPower(status);  // cycle 026: battery and mains in the header
    $("windMax").textContent = status.windStowKmh != null ? "حداکثر مجاز " + toFa(Number(status.windStowKmh).toFixed(0)) + " کیلومتر بر ساعت" : "";
    $("windNote").textContent = status.windSensorFault ? "بادسنج خطا می‌دهد؛ سیم و سنسور را ببینید" : status.windStow ? "حالت ایمن باد" : "";
    $("rowWind").classList.toggle("bad", !!status.windSensorFault);
    $("segAuto").classList.toggle("on", status.mode !== "manual"); $("segMan").classList.toggle("on", status.mode === "manual");
    $("state").textContent = { Auto: "ردیابی خورشید", Manual: "دستی", Wind: "حالت ایمن باد", Night: "شب", Fault: "خطا" }[status.state] || status.state;
    $("stateName").textContent = status.moving ? "در حال حرکت" : "ایستاده";
    $("fault").textContent = status.fault ? "خطا" : "—";
    $("faultText").textContent = status.fault ? status.text : "بدون خطا";
    $("statusText").textContent = status.text;
    const sid = status.statusId;
    const badge = status.fault ? "red" : (status.state === "Wind" || status.state === "Manual" || sid === 9 || sid === 10 || sid === 11 || sid === 15) ? "amber" : "ok";
    $("statusBadge").textContent = status.fault ? "خطا" : badge === "amber" ? "توجه" : "بدون خطا";
    $("statusBadge").className = "badge " + badge;
    $("fw").textContent = status.fwVersion;
    if (status.clockValid) { clockBase = { utc: status.utc, tz: status.tzOffsetMin, at: Date.now() }; renderClock(); }
    else { clockBase = null; $("clockNow").textContent = "تنظیم نشده"; $("liveTime").classList.add("hidden"); $("liveOff").classList.remove("hidden"); $("liveDate").textContent = ""; }
    const hasSun = status.sunElevationDeg !== null && status.sunElevationDeg !== undefined;
    $("sunEl").innerHTML = hasSun ? degTxt(status.sunElevationDeg) : "—";
    $("sunNote").innerHTML = !hasSun ? "" : (status.sunElevationDeg < 0 ? "زیر افق · " : "") + (status.sunMaxElevationDeg != null ? "اوج امروز " + degTxt(status.sunMaxElevationDeg) : "");
    netFromStatus();
    renderWifi(status);
    $("warnDefault").classList.toggle("hidden", !status.passwordIsDefault);
    $("warnClock").classList.toggle("hidden", status.clockValid);
    $("warnCal").classList.toggle("hidden", status.calibrated);
    $("warnReversed").classList.toggle("hidden", !status.reversedHint);
    $("warnAngle").classList.toggle("hidden", !status.angleWarning);
    if (status.angleWarning && status.liveAngleDeg != null) $("warnAngleVal").innerHTML = degTxt(status.liveAngleDeg);
    const live = status.liveAngleDeg;
    $("liveAngle").innerHTML = live == null ? "زاویهٔ زنده: — (کالیبراسیون کامل نیست یا سنسور داده نمی‌دهد)"
      : "زاویهٔ زنده: " + degTxt(live) + (status.angleWarning ? " — <b>خارج از محدودهٔ عادی</b>" : " — در محدوده");
    if (!$("calib").classList.contains("hidden")) refreshOffsets();
    if (document.activeElement !== $("clockAuto")) $("clockAuto").checked = !!status.clockAuto;
    autoClockSync();
    if (!status.calibrated && status.gravity) { const g = status.gravity.map((v) => v.toFixed(2)); $("rawG").textContent = "سنسور زنده: x=" + toFa(g[0]) + " y=" + toFa(g[1]) + " z=" + toFa(g[2]); }
    $("faultBar").classList.toggle("hidden", !status.fault);
    $("faultBarText").textContent = status.fault ? status.text : "";
    $("resetFault").classList.toggle("hidden", !(status.fault || status.windStow || status.windSensorFault) || !loggedIn);
    moveGauge(uiAngle(status.angleDeg));
    moveSun(status.sunElevationDeg, status.sunAzimuthDeg);
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
  if (!(await confirmDialog("ریست شود؟ مطمئن شوید علت آن (مثلاً گیر کردن بازو) برطرف شده است. اگر حالت ایمن باد فعال باشد هم تمام می‌شود و اگر باد واقعاً شدید باشد، بعد از چند ثانیه دوباره شروع می‌شود."))) return;
  const r = await api("POST", "/api/fault/reset", {});
  toast(r.ok ? "خطا ریست شد" : errText(r), r.ok ? "ok" : "err"); poll();
};

// ---- restart (cycle 021) ----
$("restartBtn").onclick = async () => {
  if (!(await confirmDialog("دستگاه دوباره راه‌اندازی شود؟ اگر در حال حرکت باشد اول می‌ایستد."))) return;
  const r = await api("POST", "/api/restart", {});
  toast(r.ok ? "در حال ریستارت… چند ثانیه صبر کنید" : errText(r), r.ok ? "ok" : "err");
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
for (const [id, dir] of [["moveEast", "east"], ["moveWest", "west"], ["dMoveEast", "east"], ["dMoveWest", "west"]]) {
  const b = $(id);
  const up = () => { b.classList.remove("down"); release(); };
  b.onpointerdown = (e) => { e.preventDefault(); b.setPointerCapture(e.pointerId); b.classList.add("down"); move(dir); };
  b.onpointerup = up; b.onpointercancel = up; b.onlostpointercapture = up;
  b.oncontextmenu = (e) => e.preventDefault();
}
$("moveStop").onclick = release;
$("dMoveStop").onclick = release;
// The dashboard shows its own move buttons while the manual mode is on (and the user is logged in).
function dashMoveVisible() { $("dashMove").classList.toggle("hidden", !(loggedIn && status && status.mode === "manual")); }
setInterval(dashMoveVisible, 500);

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

// ---- sensor offsets: the unfolded cube (cycle 018) ----
// [label, column, row] in the cross layout; the index is the face number the device uses (2*axis + negative).
const FACES = [["X+", 2, 1], ["X−", 0, 1], ["Y+", 1, 0], ["Y−", 1, 2], ["Z+", 1, 1], ["Z−", 3, 1]];
let offState = { faces: 0, capturing: false, saved: false };
// Which face the device stands on right now, from the live (raw) reading; -1 when it is tilted between faces.
function faceNow(g) {
  const a = g.map(Math.abs); let k = 0;
  if (a[1] > a[k]) k = 1;
  if (a[2] > a[k]) k = 2;
  if (a[k] < 0.7 || a.some((v, i) => i !== k && v > 0.35)) return -1;
  return 2 * k + (g[k] < 0 ? 1 : 0);
}
function drawCube() {
  const g = status && status.gravity, now = g ? faceNow(g) : -1;
  $("cube").innerHTML = FACES.map(([label, c, r], i) => {
    const done = (offState.faces >> i) & 1, here = i === now;
    const stroke = here ? "var(--accent)" : done ? "var(--ok)" : "var(--line)";
    const fill = here ? "var(--accent)" : done ? "var(--ok)" : "var(--card)";
    const sub = here ? "الان" : done ? "✓ ثبت شد" : "";
    return `<rect x="${c * 68 + 2}" y="${r * 68 + 2}" width="64" height="64" rx="8" fill="${fill}" fill-opacity="${here || done ? 0.28 : 1}" stroke="${stroke}" stroke-width="${here ? 4 : 2}"/>` +
      `<text x="${c * 68 + 34}" y="${r * 68 + 38}" text-anchor="middle" font-size="22" font-weight="700" fill="var(--ink)">${label}</text>` +
      `<text x="${c * 68 + 34}" y="${r * 68 + 56}" text-anchor="middle" font-size="11" fill="var(--mute)">${sub}</text>`;
  }).join("");
  if (g) $("liveG").innerHTML = "سنسور زنده: " + ltr("x=" + toFa(g[0].toFixed(2)) + " y=" + toFa(g[1].toFixed(2)) + " z=" + toFa(g[2].toFixed(2))) + (now < 0 ? " — روی یک وجه صاف نیست" : "");
  $("offCap").disabled = now < 0 || offState.capturing;
  $("offSave").disabled = offState.faces !== 63;
}
async function refreshOffsets() {
  const r = await api("GET", "/api/offsets");
  if (r.ok) offState = r.data;
  drawCube();
}
const OFF = { pending: "در حال ثبت… دستگاه را ثابت نگه دارید", ok: "ثبت شد", moving: "دستگاه حرکت کرد؛ دوباره امتحان کنید", "bad-face": "دستگاه روی یک وجه صاف نیست",
  "bad-gravity": "داده سنسور درست نیست", "sensor-fault": "سنسور زاویه پاسخ نمی‌دهد", "not-captured": "" };
$("offCap").onclick = async () => {
  const r = await api("POST", "/api/offsets/capture", {});
  if (!r.ok) { say($("offMsg"), errText(r), "err"); return; }
  offState.capturing = true;
  for (let i = 0; i < 40; i++) {
    const s = await api("GET", "/api/offsets");
    if (s.ok) offState = s.data;
    say($("offMsg"), OFF[s.data.result] || "", s.data.result === "pending" ? "" : (s.data.result === "ok" ? "ok" : "err"));
    drawCube();
    if (s.data.result !== "pending") return;
    await new Promise((res) => setTimeout(res, 500));
  }
};
$("offSave").onclick = async () => {
  const r = await api("POST", "/api/offsets/save", {});
  toast(r.ok ? "آفست‌ها ذخیره شد؛ کالیبراسیون زاویه را دوباره انجام دهید" : errText(r), r.ok ? "ok" : "err");
  refreshOffsets(); poll();
};
$("offReset").onclick = async () => {
  if (!confirm("آفست‌های ذخیره‌شده و وجه‌های ثبت‌شده پاک شود؟ کالیبراسیون زاویه هم باید دوباره انجام شود.")) return;
  const r = await api("POST", "/api/offsets/reset", {});
  toast(r.ok ? "پاک شد" : errText(r), r.ok ? "ok" : "err");
  say($("offMsg"), "", ""); refreshOffsets(); poll();
};

// ---- calibration ----
const CAL = { pending: "در حال ثبت… پنل را ثابت نگه دارید", ok: "ثبت شد", moving: "کالیبراسیون نامعتبر: پنل در حال حرکت بود", "too-small-angle": "کالیبراسیون نامعتبر: زاویه دو مرحله خیلی کم است",
  "too-large-angle": "کالیبراسیون نامعتبر: زاویه دو مرحله خیلی زیاد است", "bad-gravity": "کالیبراسیون نامعتبر: داده سنسور درست نیست؛ اگر آفست‌گیری نکرده‌اید اول آن را کامل کنید", "sensor-fault": "سنسور زاویه پاسخ نمی‌دهد", "not-captured": "" };
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
const FAULTS = ["Fault", "FaultReset"];
const TYPE_FA = { PowerUp: "روشن شدن", StateChange: "تغییر وضعیت", Fault: "خطا", FaultReset: "ریست خطا", MoveStart: "شروع حرکت", MoveEnd: "پایان حرکت", ClockSet: "تنظیم ساعت",
  SettingsChange: "تغییر تنظیمات", SettingsReset: "بازنشانی تنظیمات", SettingsMigrated: "به‌روزرسانی ساختار تنظیمات", SupplyLow: "ولتاژ پایین", LogReset: "پاک شدن لاگ" };
const DETAIL_FA = [["no stored settings", "تنظیمات ذخیره‌شده‌ای نبود؛ پیش‌فرض‌ها بارگذاری شد"], ["power-on after cut", "روشن شدن پس از قطع برق"], ["power-on", "روشن شدن دستگاه"], ["wind stow start", "شروع حالت ایمن باد"], ["wind stow end", "پایان حالت ایمن باد"],
  ["end of stroke: limit reached", "انتهای کورس جک: به حد رسید"], ["manual keep-alive lost", "ارتباط دکمه دستی قطع شد"], ["angle calibration saved", "کالیبراسیون زاویه ذخیره شد"], ["sensor offsets saved", "آفست سنسور ذخیره شد"], ["sensor offsets reset", "آفست سنسور پاک شد"], ["daily:", "خلاصهٔ روز:"], ["moves=", "تعداد حرکت: "], ["run_s=", "زمان کار موتور (ثانیه): "], ["wind_max=", "بیشترین باد: "], ["bat_min=", "کمترین باتری: "], ["err_max=", "بیشترین فاصله از هدف (درجه): "], ["angle warning start", "هشدار زاویه شروع شد"], ["angle warning end", "هشدار زاویه تمام شد"], ["bale message failed", "ارسال پیام بله ناموفق بود"], ["bale messages work again", "ارسال پیام بله دوباره کار می‌کند"], ["mode auto from the panel", "حالت اتوماتیک از پنل انتخاب شد"], ["mode manual from the panel", "حالت دستی از پنل انتخاب شد"], ["wind sensor fault: unbelievable readings", "بادسنج خطا می‌دهد: سرعت غیرممکن یا نوسان شدید"], ["wind sensor readings believable again", "بادسنج دوباره عادی شد"], ["wind stow reset from the panel", "حالت ایمن باد با دکمهٔ ریست پایان یافت"], ["power: mains lost", "برق شهر قطع شد"], ["power: mains restored", "برق شهر وصل شد"], ["power: battery low", "باتری کم است"], ["power: battery critical", "باتری تقریباً خالی است"], ["power: battery recovered", "باتری به حالت عادی برگشت"],
  ["notification settings changed", "تنظیمات هشدار عوض شد"], ["settings password changed", "رمز عوض شد"], ["site wifi changed", "وای‌فای محل عوض شد"], ["older layout", "ساختار قدیمی"],
  ["dir=east", "جهت: شرق"], ["dir=west", "جهت: غرب"], ["reason=", "علت: "], ["angle=", "زاویه: "], ["ms=", "مدت (میلی‌ثانیه): "], ["->", " ⟵ "],
  ["Auto", "خودکار"], ["Manual", "دستی"], ["Wind", "باد"], ["Night", "شب"], ["Fault", "خطا"], ["stop", "توقف"], ["target", "رسیدن به هدف"]];
function detailFa(d) { let t = String(d).replace(/[<&]/g, "").replace(/angle=(-?\d+(?:\.\d+)?)/g, (m, v) => "angle=" + uiAngle(parseFloat(v)).toFixed(1)); for (const [a, b] of DETAIL_FA) t = t.split(a).join(b); return toFa(t); }
function renderLog() {
  const tz = status ? status.tzOffsetMin : 0;
  const keep = (e) => logFilter === "all" || (logFilter === "fault" && FAULTS.includes(e.type)) || (logFilter === "system" && !FAULTS.includes(e.type));
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
// Wi-Fi icon (cycle 028): the fan of arcs, not the phone's rising bars. 3 arcs = strong, 0 arcs = only the dot.
const wifiLevel = (rssi) => (rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -80 ? 1 : 0);
const wifiSvg = (level, off) => `<svg class="wf l${level}${off ? " off" : ""}" viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" aria-hidden="true"><path class="a3" d="M2.5 9.2a13.5 13.5 0 0 1 19 0"/><path class="a2" d="M5.6 12.6a9.1 9.1 0 0 1 12.8 0"/><path class="a1" d="M8.7 16a4.7 4.7 0 0 1 6.6 0"/><circle cx="12" cy="19.6" r="1.4" fill="currentColor" stroke="none"/><path class="slash" d="M3 3l18 18"/></svg>`;
let wifiSeen = false;  // the header icon shows a cut connection only after it was connected once
function renderWifi(st) {
  const c = !!st.staConnected;
  if (c) wifiSeen = true;
  $("wifi").classList.toggle("hidden", !c && !wifiSeen);
  const rssi = st.staRssi || -100;
  $("wifi").innerHTML = wifiSvg(c ? wifiLevel(rssi) : 0, !c);
  $("wifi").className = "wifi" + (c ? "" : " bad") + (!c && !wifiSeen ? " hidden" : "");
  $("wifi").title = c ? "وای‌فای محل: متصل، قدرت سیگنال " + toFa(rssi) + " دسی‌بل‌میلی‌وات (dBm)، آدرس " + st.staIp : "وای‌فای محل قطع است";
}
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
  $("netBars").innerHTML = connected && rssi ? wifiSvg(wifiLevel(rssi)) + " " + ltr(toFa(rssi) + " dBm") : "";
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
        ? r.data.networks.map((n) => `<li data-ssid="${n.ssid.replace(/"/g, "&quot;")}"><span>${n.ssid.replace(/[<&]/g, "")}</span><span class="bars">${wifiSvg(wifiLevel(n.rssi))} ${n.secure ? "🔒" : ""}</span></li>`).join("")
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
