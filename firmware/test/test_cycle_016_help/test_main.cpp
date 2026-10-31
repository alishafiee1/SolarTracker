// Cycle 016 — every parameter has a help text and its numbers agree with the firmware
// (rodo/suntracker/cycles/016-parameter-help). Reads the panel files from disk.
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include <functional>
#include <string>

#include "settings.h"

using settings::Settings;

void setUp() {}
void tearDown() {}

static std::string readFile(const char* name) {
    const char* prefixes[] = {"web/", "firmware/web/", "../web/"};
    for (const char* p : prefixes) {
        FILE* f = fopen((std::string(p) + name).c_str(), "rb");
        if (!f) continue;
        std::string s;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
        fclose(f);
        return s;
    }
    return "";
}

static JsonDocument loadHelp() {
    const std::string js = readFile("app.js");
    TEST_ASSERT_TRUE_MESSAGE(!js.empty(), "web/app.js not found");
    const size_t a = js.find("/*HELP-BEGIN*/");
    const size_t b = js.find("/*HELP-END*/");
    TEST_ASSERT_TRUE(a != std::string::npos && b != std::string::npos && b > a);
    const std::string json = js.substr(a + strlen("/*HELP-BEGIN*/"), b - a - strlen("/*HELP-BEGIN*/"));
    JsonDocument d;
    TEST_ASSERT_TRUE_MESSAGE(!deserializeJson(d, json), "the help block is not valid JSON");
    return d;
}

// One numeric setting: how to write it and what must be arranged first for the limits to be independent of the others.
struct Num {
    const char* key;
    std::function<void(Settings&, double)> set;
    std::function<double(const Settings&)> get;
    std::function<void(Settings&)> prepLow;   // make the lower limit testable (other coupled fields out of the way)
    std::function<void(Settings&)> prepHigh;  // same for the upper limit
    bool integer;
};

static const Num kNums[] = {
    {"moveThresholdDeg", [](Settings& s, double v) { s.moveThresholdDeg = (float)v; }, [](const Settings& s) { return (double)s.moveThresholdDeg; }, nullptr, nullptr, false},
    {"minDeg", [](Settings& s, double v) { s.minDeg = (float)v; }, [](const Settings& s) { return (double)s.minDeg; }, nullptr, nullptr, false},
    {"maxDeg", [](Settings& s, double v) { s.maxDeg = (float)v; }, [](const Settings& s) { return (double)s.maxDeg; }, nullptr, nullptr, false},
    {"preDawnMinutes", [](Settings& s, double v) { s.preDawnMinutes = (uint32_t)v; }, [](const Settings& s) { return (double)s.preDawnMinutes; }, nullptr, nullptr, true},
    {"stallCheckMs", [](Settings& s, double v) { s.stallCheckMs = (uint32_t)v; }, [](const Settings& s) { return (double)s.stallCheckMs; }, nullptr, nullptr, true},
    {"stopSettleMs", [](Settings& s, double v) { s.stopSettleMs = (uint32_t)v; }, [](const Settings& s) { return (double)s.stopSettleMs; }, nullptr, nullptr, true},
    {"minMoveGapMs", [](Settings& s, double v) { s.minMoveGapMs = (uint32_t)v; }, [](const Settings& s) { return (double)s.minMoveGapMs; }, nullptr, nullptr, true},
    {"maxMoveMs", [](Settings& s, double v) { s.maxMoveMs = (uint32_t)v; }, [](const Settings& s) { return (double)s.maxMoveMs; }, nullptr, nullptr, true},
    {"windStowKmh", [](Settings& s, double v) { s.windStowKmh = (float)v; }, [](const Settings& s) { return (double)s.windStowKmh; }, [](Settings& s) { s.windReturnKmh = 5; }, nullptr, false},
    {"windStowDelayS", [](Settings& s, double v) { s.windStowDelayS = (uint32_t)v; }, [](const Settings& s) { return (double)s.windStowDelayS; }, nullptr, nullptr, true},
    {"windReturnKmh", [](Settings& s, double v) { s.windReturnKmh = (float)v; }, [](const Settings& s) { return (double)s.windReturnKmh; }, nullptr, [](Settings& s) { s.windStowKmh = 100; }, false},
    {"windReturnMin", [](Settings& s, double v) { s.windReturnMin = (uint32_t)v; }, [](const Settings& s) { return (double)s.windReturnMin; }, nullptr, nullptr, true},
    {"anemometerFactor", [](Settings& s, double v) { s.anemometerFactor = (float)v; }, [](const Settings& s) { return (double)s.anemometerFactor; }, nullptr, nullptr, false},
    {"supplyLowV", [](Settings& s, double v) { s.supplyLowV = (float)v; }, [](const Settings& s) { return (double)s.supplyLowV; }, nullptr, nullptr, false},
    {"currentFactor", [](Settings& s, double v) { s.currentFactor = (float)v; }, [](const Settings& s) { return (double)s.currentFactor; }, nullptr, nullptr, false},
    {"stallCurrentA", [](Settings& s, double v) { s.stallCurrentA = (float)v; }, [](const Settings& s) { return (double)s.stallCurrentA; }, nullptr, [](Settings& s) { s.maxCurrentA = 30; }, false},
    {"maxCurrentA", [](Settings& s, double v) { s.maxCurrentA = (float)v; }, [](const Settings& s) { return (double)s.maxCurrentA; }, [](Settings& s) { s.stallCurrentA = 0.5f; }, nullptr, false},
    {"zeroCurrentA", [](Settings& s, double v) { s.zeroCurrentA = (float)v; }, [](const Settings& s) { return (double)s.zeroCurrentA; }, nullptr, nullptr, false},
};

static std::string lastInvalid;
static bool valid(const Settings& s) {
    const char* names[4] = {nullptr, nullptr, nullptr, nullptr};
    const size_t n = settings::validate(s, names, 4);
    lastInvalid.clear();
    for (size_t i = 0; i < n; ++i) lastInvalid += std::string(names[i]) + " ";
    return n == 0;
}

static void help_numbers_match_the_validation_and_the_defaults() {
    JsonDocument help = loadHelp();
    const Settings defaults;
    for (const Num& n : kNums) {
        JsonObject h = help[n.key].as<JsonObject>();
        char msg[96];
        snprintf(msg, sizeof(msg), "no help entry for %s", n.key);
        TEST_ASSERT_TRUE_MESSAGE(!h.isNull(), msg);
        // ArduinoJson keeps small decimals as float (0.05 reads back as 0.049999997): round to the 4 decimals used in the texts.
        auto tidy = [](double x) { return std::round(x * 10000.0) / 10000.0; };
        const double lo = tidy(h["min"].as<double>()), hi = tidy(h["max"].as<double>()), def = tidy(h["def"].as<double>());
        const double step = n.integer ? 1.0 : 0.01;

        snprintf(msg, sizeof(msg), "%s: help default %g but the firmware default is %g", n.key, def, n.get(defaults));
        TEST_ASSERT_TRUE_MESSAGE(fabs(n.get(defaults) - def) < 1e-4, msg);

        Settings s;
        if (n.prepLow) n.prepLow(s);
        n.set(s, lo);
        const bool lowOk = valid(s);
        snprintf(msg, sizeof(msg), "%s: the lower limit %.9g in the help is refused by the firmware (invalid: %s)", n.key, lo, lastInvalid.c_str());
        TEST_ASSERT_TRUE_MESSAGE(lowOk, msg);
        n.set(s, lo - step);
        snprintf(msg, sizeof(msg), "%s: a value below the lower limit %g is accepted by the firmware", n.key, lo);
        TEST_ASSERT_FALSE_MESSAGE(valid(s), msg);

        Settings t;
        if (n.prepHigh) n.prepHigh(t);
        n.set(t, hi);
        snprintf(msg, sizeof(msg), "%s: the upper limit %g in the help is refused by the firmware", n.key, hi);
        TEST_ASSERT_TRUE_MESSAGE(valid(t), msg);
        n.set(t, hi + step);
        snprintf(msg, sizeof(msg), "%s: a value above the upper limit %g is accepted by the firmware", n.key, hi);
        TEST_ASSERT_FALSE_MESSAGE(valid(t), msg);
    }
}

static void every_help_button_in_the_page_has_a_text() {
    JsonDocument help = loadHelp();
    const std::string html = readFile("index.html");
    const std::string js = readFile("app.js");
    TEST_ASSERT_TRUE(!html.empty() && !js.empty());
    // Static buttons: data-help="key" in index.html.
    size_t pos = 0;
    int found = 0;
    while ((pos = html.find("data-help=\"", pos)) != std::string::npos) {
        pos += strlen("data-help=\"");
        const std::string key = html.substr(pos, html.find('"', pos) - pos);
        char msg[96];
        snprintf(msg, sizeof(msg), "index.html has a help button for '%s' without a text", key.c_str());
        TEST_ASSERT_TRUE_MESSAGE(!help[key].isNull(), msg);
        // The box that opens must exist too.
        TEST_ASSERT_TRUE_MESSAGE(html.find("id=\"h_" + key + "\"") != std::string::npos, key.c_str());
        ++found;
    }
    TEST_ASSERT_TRUE(found >= 9);
    // Fields built by the script: every key of the settings groups.
    const size_t g = js.find("const GROUPS");
    TEST_ASSERT_TRUE(g != std::string::npos);
    const size_t end = js.find("const ALL_KEYS", g);
    pos = g;
    int groupKeys = 0;
    while ((pos = js.find("[\"", pos)) != std::string::npos && pos < end) {
        pos += 2;
        const size_t q = js.find('"', pos);
        const std::string key = js.substr(pos, q - pos);
        if (key.size() > 3 && key.find(' ') == std::string::npos && key.find("،") == std::string::npos && !help[key].isNull()) ++groupKeys;
        else if (key.size() > 3 && key.find(' ') == std::string::npos && (key.find("Deg") != std::string::npos || key.find("Ms") != std::string::npos || key.find("Kmh") != std::string::npos)) {
            char msg[96];
            snprintf(msg, sizeof(msg), "settings field '%s' has no help text", key.c_str());
            TEST_FAIL_MESSAGE(msg);
        }
    }
    TEST_ASSERT_TRUE(groupKeys >= 19);
}

static void help_texts_are_real_sentences() {
    JsonDocument help = loadHelp();
    for (JsonPair kv : help.as<JsonObject>()) {
        const char* t = kv.value()["t"] | "";
        char msg[96];
        snprintf(msg, sizeof(msg), "help for %s is too short", kv.key().c_str());
        TEST_ASSERT_TRUE_MESSAGE(strlen(t) > 60, msg);  // in UTF-8 a Persian sentence of 25+ characters
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(help_numbers_match_the_validation_and_the_defaults);
    RUN_TEST(every_help_button_in_the_page_has_a_text);
    RUN_TEST(help_texts_are_real_sentences);
    return UNITY_END();
}
