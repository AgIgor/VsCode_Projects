/**
 * ESP32 YAML GPIO Config
 * ─────────────────────────────────────────────────────────────────────────────
 * Web-based GPIO configurator via YAML.  Supports:
 *
 *  gpio:
 *    mode: output | input | input_pullup | input_pulldown
 *    state: low | high          (output initial state)
 *    delay_on_ms:  <N>          (ms before turning ON  – 0 = immediate)
 *    delay_off_ms: <N>          (ms before turning OFF – 0 = immediate)
 *    counter:                   (pulse counter on input/output pins)
 *      edge: falling | rising | change
 *      debounce_ms: <N>
 *
 *  rules:                       (automation – evaluated every 100 ms)
 *    trigger: change | always
 *    if:
 *      pin: <N>
 *      state: high | low        (read digitalRead)
 *      # OR counter conditions:
 *      counter_gte: <N>         (counter >= N)
 *      counter_lte: <N>         (counter <= N)
 *      counter_eq:  <N>         (counter == N)
 *    then:
 *      pin: <N>
 *      action: high | low | toggle
 *      reset_counter: true      (reset condition-pin counter after action)
 *    else:                      (optional)
 *      pin: <N>
 *      action: high | low | toggle
 *
 * Default access: http://192.168.4.1  (SSID: ESP32-Config / esp32config)
 * Filesystem:     pio run --target uploadfs
 */

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <YAMLDuino.h>   // pulls in ArduinoJson 7.x automatically

// ─── Constants ───────────────────────────────────────────────────────────────
static const char* AP_SSID_DEF = "ESP32-Config";
static const char* AP_PASS_DEF = "esp32config";
static const char* CONFIG_FILE = "/config.yaml";

// ─── ISR counters (indexed by GPIO pin 0-39) ─────────────────────────────────
static volatile uint32_t g_pulseCount[40];
static volatile uint32_t g_lastEdgeUs[40];   // micros() at last edge
static          uint32_t g_debounceUs[40];   // debounce window in µs

void IRAM_ATTR pulseISR(void* arg) {
    uint8_t  pin = (uint8_t)(uintptr_t)arg;
    uint32_t now = (uint32_t)micros();
    if (now - g_lastEdgeUs[pin] >= g_debounceUs[pin]) {
        g_pulseCount[pin]++;
        g_lastEdgeUs[pin] = now;
    }
}

// ─── Structures ──────────────────────────────────────────────────────────────
struct GpioPin {
    uint8_t  pin;
    String   name;
    String   mode;           // output | input | input_pullup | input_pulldown
    uint32_t delayOnMs  = 0; // ms before turning ON  (0 = immediate)
    uint32_t delayOffMs = 0; // ms before turning OFF (0 = immediate)
    bool     pendingActive  = false;
    int      pendingState   = 0;
    uint32_t pendingAt      = 0; // millis() when to fire
    bool     counterEnabled = false;
};

struct RuleCondition {
    uint8_t pin   = 0;
    String  type  = "state";  // state | counter_gte | counter_lte | counter_eq
    int     value = 0;        // state: HIGH=1/LOW=0 ; counter: threshold
};

struct RuleAction {
    int8_t  pin           = -1;  // -1 = no-op
    String  action        = "high"; // high | low | toggle
    bool    resetCounter  = false;  // reset condition-pin counter after action
};

struct Rule {
    String        name;
    bool          triggerOnChange = true; // false = always re-evaluate
    RuleCondition condition;
    RuleAction    thenAct;
    bool          hasElse = false;
    RuleAction    elseAct;
    bool          lastCondState = false;  // for edge detection
};

// ─── Globals ─────────────────────────────────────────────────────────────────
AsyncWebServer server(80);
static std::vector<GpioPin> gpioPins;
static std::vector<Rule>    rules;

// Shared POST body buffer (single-user device)
static char   g_buf[8192];
static size_t g_bufLen;

// ─── File helpers ─────────────────────────────────────────────────────────────
static String readFile(const char* path) {
    File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) return "";
    String s = f.readString();
    f.close();
    return s;
}
static bool writeFile(const char* path, const String& data) {
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    f.print(data);
    f.close();
    return true;
}

// ─── POST body helpers ────────────────────────────────────────────────────────
static void bodyReset()                              { g_bufLen = 0; }
static void bodyAppend(const uint8_t* d, size_t n)  {
    size_t cp = n < (sizeof(g_buf) - 1 - g_bufLen) ? n : (sizeof(g_buf) - 1 - g_bufLen);
    memcpy(g_buf + g_bufLen, d, cp);  g_bufLen += cp;
}
static String bodyString() { g_buf[g_bufLen] = '\0'; return String(g_buf); }

// ─── YAML → JsonDocument ──────────────────────────────────────────────────────
static String parseYaml(const String& yaml, JsonDocument& doc) {
    auto err = deserializeYml(doc, yaml.c_str());
    if (err) return String("YAML error: ") + err.c_str();
    return "";
}

// ─── Pin lookup ───────────────────────────────────────────────────────────────
static GpioPin* findPin(uint8_t pin) {
    for (auto& p : gpioPins) if (p.pin == pin) return &p;
    return nullptr;
}

// ─── Apply state with optional ON/OFF delay ───────────────────────────────────
static void applyPinState(GpioPin& p, int newState) {
    if (p.mode != "output") return;
    uint32_t delayMs = (newState == HIGH) ? p.delayOnMs : p.delayOffMs;
    if (delayMs == 0) {
        p.pendingActive = false;
        digitalWrite(p.pin, newState);
    } else {
        p.pendingActive = true;
        p.pendingState  = newState;
        p.pendingAt     = millis() + delayMs;
    }
}

// ─── Rule engine ─────────────────────────────────────────────────────────────
static bool evalCondition(const RuleCondition& c) {
    if (c.type == "state")       return digitalRead(c.pin) == c.value;
    uint32_t cnt = g_pulseCount[c.pin];
    if (c.type == "counter_gte") return cnt >= (uint32_t)c.value;
    if (c.type == "counter_lte") return cnt <= (uint32_t)c.value;
    if (c.type == "counter_eq")  return cnt == (uint32_t)c.value;
    return false;
}

static void execAction(const RuleAction& act, uint8_t condPin) {
    if (act.pin < 0) return;
    GpioPin* p = findPin((uint8_t)act.pin);
    if (!p || p->mode != "output") return;
    int newState;
    if      (act.action == "high")   newState = HIGH;
    else if (act.action == "low")    newState = LOW;
    else if (act.action == "toggle") newState = !digitalRead(p->pin);
    else return;
    applyPinState(*p, newState);
    if (act.resetCounter && condPin < 40) g_pulseCount[condPin] = 0;
}

static void evaluateRules() {
    for (auto& rule : rules) {
        bool condMet = evalCondition(rule.condition);
        if (rule.triggerOnChange) {
            if (condMet == rule.lastCondState) continue;
            rule.lastCondState = condMet;
        }
        if (condMet)           execAction(rule.thenAct, rule.condition.pin);
        else if (rule.hasElse) execAction(rule.elseAct, rule.condition.pin);
    }
}

// ─── Configure GPIO ───────────────────────────────────────────────────────────
static void configureGpio(const JsonDocument& doc) {
    for (auto& p : gpioPins)
        if (p.counterEnabled) detachInterrupt(p.pin);
    gpioPins.clear();

    JsonArrayConst arr = doc["gpio"].as<JsonArrayConst>();
    for (JsonObjectConst g : arr) {
        int pinNum = g["pin"] | -1;
        if (pinNum < 0 || pinNum > 39) continue;

        GpioPin p;
        p.pin  = (uint8_t)pinNum;
        { const char* n = g["name"]; p.name = n ? n : ("GPIO" + String(pinNum)); }
        { const char* m = g["mode"]; p.mode = m ? m : "input"; }
        p.delayOnMs     = g["delay_on_ms"]  | 0;
        p.delayOffMs    = g["delay_off_ms"] | 0;
        p.pendingActive = false;
        p.counterEnabled = false;

        if (p.mode == "output") {
            const char* stRaw = g["state"];
            String st = stRaw ? stRaw : "low";
            digitalWrite(p.pin, (st == "high" || st == "1" || st == "true") ? HIGH : LOW);
            pinMode(p.pin, OUTPUT);
        } else if (p.mode == "input_pullup")   { pinMode(p.pin, INPUT_PULLUP);   }
        else if (p.mode == "input_pulldown")   { pinMode(p.pin, INPUT_PULLDOWN); }
        else                                   { pinMode(p.pin, INPUT);          }

        // Pulse counter
        JsonVariantConst ctrVar = g["counter"];
        if (ctrVar.is<JsonObjectConst>()) {
            JsonObjectConst ctr = ctrVar.as<JsonObjectConst>();
            const char* edgeStr = ctr["edge"] | "falling";
            uint32_t debMs      = ctr["debounce_ms"] | 50;
            int edgeMode;
            if      (strcmp(edgeStr, "rising") == 0) edgeMode = RISING;
            else if (strcmp(edgeStr, "change") == 0) edgeMode = CHANGE;
            else                                     edgeMode = FALLING;
            g_debounceUs[p.pin] = debMs * 1000UL;
            g_pulseCount[p.pin] = 0;
            g_lastEdgeUs[p.pin] = 0;
            attachInterruptArg(p.pin, pulseISR, (void*)(uintptr_t)p.pin, edgeMode);
            p.counterEnabled = true;
            Serial.printf("  GPIO%-2d [%-14s] %s | counter (%s, deb=%ums)\n",
                p.pin, p.name.c_str(), p.mode.c_str(), edgeStr, (unsigned)debMs);
        } else {
            Serial.printf("  GPIO%-2d [%-14s] %s%s%s\n",
                p.pin, p.name.c_str(), p.mode.c_str(),
                p.delayOnMs  ? " delay_on"  : "",
                p.delayOffMs ? " delay_off" : "");
        }
        gpioPins.push_back(p);
    }
    Serial.printf("  %u pin(s) OK\n", (unsigned)gpioPins.size());
}

// ─── Parse a rule action sub-object ──────────────────────────────────────────
static RuleAction parseAction(JsonObjectConst obj) {
    RuleAction a;
    a.pin = obj["pin"] | -1;
    // Normalise: YAML "on"/"off" become booleans via libyaml; handle gracefully
    JsonVariantConst av = obj["action"];
    if (av.is<bool>())              { a.action = av.as<bool>() ? "high" : "low"; }
    else if (av.is<const char*>()) {
        const char* s = av.as<const char*>();
        if (!s || strcmp(s,"on")==0)      a.action = "high";  // quote "on" in YAML!
        else if (strcmp(s,"off")==0)      a.action = "low";
        else                              a.action = String(s);
    }
    a.resetCounter = obj["reset_counter"] | false;
    return a;
}

// ─── Configure automation rules ───────────────────────────────────────────────
static void configureRules(const JsonDocument& doc) {
    rules.clear();
    JsonArrayConst arr = doc["rules"].as<JsonArrayConst>();
    for (JsonObjectConst r : arr) {
        Rule rule;
        { const char* n = r["name"]; rule.name = n ? n : "Rule"; }
        { const char* t = r["trigger"];
          rule.triggerOnChange = !(t && strcmp(t, "always") == 0); }

        // Condition
        JsonObjectConst ifObj = r["if"].as<JsonObjectConst>();
        rule.condition.pin = ifObj["pin"] | 0;

        JsonVariantConst stateVar = ifObj["state"];
        if (!stateVar.isNull()) {
            rule.condition.type = "state";
            if (stateVar.is<const char*>()) {
                const char* sv = stateVar.as<const char*>();
                rule.condition.value = (sv && (strcmp(sv,"high")==0 || strcmp(sv,"1")==0)) ? HIGH : LOW;
            } else { rule.condition.value = stateVar.as<int>(); }
        } else if (!ifObj["counter_gte"].isNull()) {
            rule.condition.type  = "counter_gte";
            rule.condition.value = ifObj["counter_gte"] | 0;
        } else if (!ifObj["counter_lte"].isNull()) {
            rule.condition.type  = "counter_lte";
            rule.condition.value = ifObj["counter_lte"] | 0;
        } else if (!ifObj["counter_eq"].isNull()) {
            rule.condition.type  = "counter_eq";
            rule.condition.value = ifObj["counter_eq"]  | 0;
        }

        if (r["then"].is<JsonObjectConst>()) rule.thenAct = parseAction(r["then"].as<JsonObjectConst>());
        if (r["else"].is<JsonObjectConst>()) { rule.hasElse = true; rule.elseAct = parseAction(r["else"].as<JsonObjectConst>()); }

        rule.lastCondState = evalCondition(rule.condition);  // init to avoid boot-time trigger
        rules.push_back(rule);
        Serial.printf("  Rule [%-14s] if pin%d %s%d → %s pin%d%s\n",
            rule.name.c_str(), rule.condition.pin,
            rule.condition.type.c_str(), rule.condition.value,
            rule.thenAct.action.c_str(), rule.thenAct.pin,
            rule.hasElse ? " (else)" : "");
    }
    Serial.printf("  %u rule(s) OK\n", (unsigned)rules.size());
}

// ─── Apply GPIO + rules from YAML (save-time, no WiFi disruption) ─────────────
static String applyConfigGpioOnly(const String& yaml) {
    JsonDocument doc;
    String err = parseYaml(yaml, doc);
    if (!err.isEmpty()) return err;
    configureGpio(doc);
    configureRules(doc);
    return "";
}

// ─── Configure WiFi (boot-time only) ─────────────────────────────────────────
static void configureWifi(const JsonDocument& doc) {
    if (!doc["wifi"].is<JsonObject>()) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID_DEF, AP_PASS_DEF);
        Serial.println("AP: http://" + WiFi.softAPIP().toString());
        return;
    }
    String wmode = doc["wifi"]["mode"] | "ap";
    String ssid  = doc["wifi"]["ssid"]     | AP_SSID_DEF;
    String pass  = doc["wifi"]["password"] | AP_PASS_DEF;
    if (wmode == "station") {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(AP_SSID_DEF, AP_PASS_DEF);
        WiFi.begin(ssid.c_str(), pass.c_str());
        Serial.print("Connecting to " + ssid);
        for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print("."); }
        Serial.println(WiFi.status() == WL_CONNECTED
            ? "\nSTA: http://" + WiFi.localIP().toString()
            : "\nFailed – AP: http://" + WiFi.softAPIP().toString());
    } else {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(ssid.c_str(), pass.c_str());
        Serial.println("AP: http://" + WiFi.softAPIP().toString());
    }
}

// ─── GPIO state JSON (includes counters & pending info) ───────────────────────
static String gpioStateJson() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& p : gpioPins) {
        JsonObject o = arr.add<JsonObject>();
        o["pin"]   = p.pin;
        o["name"]  = p.name;
        o["mode"]  = p.mode;
        o["state"] = (int)digitalRead(p.pin);
        if (p.counterEnabled) o["counter"] = (uint32_t)g_pulseCount[p.pin];
        if (p.delayOnMs)      o["delay_on_ms"]  = p.delayOnMs;
        if (p.delayOffMs)     o["delay_off_ms"] = p.delayOffMs;
        if (p.pendingActive) {
            o["pending_state"] = p.pendingState;
            int32_t rem = (int32_t)(p.pendingAt - millis());
            o["pending_ms"] = rem > 0 ? rem : 0;
        }
    }
    String out; serializeJson(doc, out); return out;
}

// ─── Rules state JSON ─────────────────────────────────────────────────────────
static String rulesJson() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& rule : rules) {
        JsonObject o = arr.add<JsonObject>();
        o["name"]    = rule.name;
        o["trigger"] = rule.triggerOnChange ? "change" : "always";
        o["active"]  = rule.lastCondState;
        JsonObject cond  = o["if"].to<JsonObject>();
        cond["pin"]   = rule.condition.pin;
        cond["type"]  = rule.condition.type;
        cond["value"] = rule.condition.value;
        JsonObject then_ = o["then"].to<JsonObject>();
        then_["pin"]    = rule.thenAct.pin;
        then_["action"] = rule.thenAct.action;
        if (rule.hasElse) {
            JsonObject else_ = o["else"].to<JsonObject>();
            else_["pin"]    = rule.elseAct.pin;
            else_["action"] = rule.elseAct.action;
        }
    }
    String out; serializeJson(doc, out); return out;
}

// ─── Default YAML (written on first boot) ─────────────────────────────────────
static const char DEFAULT_CONFIG[] =
    "# ESP32 YAML GPIO Config\n"
    "#\n"
    "# gpio modes : output, input, input_pullup, input_pulldown\n"
    "# state      : low | high        (output initial state)\n"
    "# delay_on_ms / delay_off_ms     (delayed transitions in ms)\n"
    "# counter.edge  : falling | rising | change\n"
    "# counter.debounce_ms            (debounce window in ms)\n"
    "#\n"
    "# rules trigger: change (default) | always\n"
    "# rules if    : state: high/low  OR  counter_gte/lte/eq: N\n"
    "# rules action: high | low | toggle   (use quotes for 'on'/'off'!)\n"
    "# reset_counter: true             (resets condition pin counter)\n"
    "\n"
    "wifi:\n"
    "  mode: ap\n"
    "  ssid: ESP32-Config\n"
    "  password: esp32config\n"
    "\n"
    "gpio:\n"
    "  - pin: 2\n"
    "    name: LED\n"
    "    mode: output\n"
    "    state: low\n"
    "    delay_on_ms: 500\n"
    "    delay_off_ms: 200\n"
    "  - pin: 4\n"
    "    name: Button\n"
    "    mode: input_pullup\n"
    "  - pin: 18\n"
    "    name: Sensor\n"
    "    mode: input\n"
    "    counter:\n"
    "      edge: falling\n"
    "      debounce_ms: 50\n"
    "\n"
    "rules:\n"
    "  - name: ButtonToLED\n"
    "    trigger: change\n"
    "    if:\n"
    "      pin: 4\n"
    "      state: low\n"
    "    then:\n"
    "      pin: 2\n"
    "      action: high\n"
    "    else:\n"
    "      pin: 2\n"
    "      action: low\n"
    "  - name: SensorCount\n"
    "    trigger: change\n"
    "    if:\n"
    "      pin: 18\n"
    "      counter_gte: 10\n"
    "    then:\n"
    "      pin: 2\n"
    "      action: toggle\n"
    "      reset_counter: true\n";

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32 YAML GPIO Config ===");

    memset((void*)g_pulseCount, 0, sizeof(g_pulseCount));
    memset((void*)g_lastEdgeUs, 0, sizeof(g_lastEdgeUs));
    memset(g_debounceUs,        0, sizeof(g_debounceUs));

    if (!LittleFS.begin(true)) { Serial.println("[ERROR] LittleFS failed"); return; }
    Serial.println("LittleFS OK");

    if (!LittleFS.exists(CONFIG_FILE)) {
        writeFile(CONFIG_FILE, DEFAULT_CONFIG);
        Serial.println("Default config created");
    }

    String yaml = readFile(CONFIG_FILE);
    JsonDocument doc;
    String err = parseYaml(yaml, doc);
    if (!err.isEmpty()) {
        Serial.println("[WARN] " + err + " – AP fallback");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID_DEF, AP_PASS_DEF);
    } else {
        configureWifi(doc);
        configureGpio(doc);
        configureRules(doc);
    }

    // ── Routes ───────────────────────────────────────────────────────────────
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        LittleFS.exists("/index.html")
            ? req->send(LittleFS, "/index.html", "text/html")
            : req->send(200, "text/html",
                "<!DOCTYPE html><html><body><h2>ESP32 YAML Config</h2>"
                "<p>Run: <code>pio run --target uploadfs</code></p></body></html>");
    });

    server.on("/config", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/plain; charset=utf-8", readFile(CONFIG_FILE));
    });

    server.on("/config", HTTP_POST,
        [](AsyncWebServerRequest* req) {}, nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) bodyReset();
            bodyAppend(data, len);
            if (index + len < total) return;
            String yaml = bodyString();
            String err  = applyConfigGpioOnly(yaml);
            if (!err.isEmpty()) { req->send(400, "text/plain", err); return; }
            writeFile(CONFIG_FILE, yaml);
            req->send(200, "text/plain", "Saved! (WiFi changes need restart)");
        });

    server.on("/gpio/state", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", gpioStateJson());
    });

    server.on("/gpio/toggle", HTTP_POST,
        [](AsyncWebServerRequest* req) {}, nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) bodyReset();
            bodyAppend(data, len);
            if (index + len < total) return;
            JsonDocument doc;
            if (deserializeJson(doc, bodyString()) != DeserializationError::Ok) {
                req->send(400, "text/plain", "Bad JSON"); return;
            }
            int pinNum = doc["pin"] | -1;
            GpioPin* p = findPin((uint8_t)pinNum);
            if (p && p->mode == "output") {
                applyPinState(*p, !digitalRead(p->pin));
                req->send(200, "text/plain", "OK");
            } else {
                req->send(404, "text/plain", "Output pin not found");
            }
        });

    server.on("/counter/reset", HTTP_POST,
        [](AsyncWebServerRequest* req) {}, nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) bodyReset();
            bodyAppend(data, len);
            if (index + len < total) return;
            JsonDocument doc;
            if (deserializeJson(doc, bodyString()) != DeserializationError::Ok) {
                req->send(400, "text/plain", "Bad JSON"); return;
            }
            int pin = doc["pin"] | -1;
            if (pin >= 0 && pin < 40) {
                g_pulseCount[pin] = 0;
                req->send(200, "text/plain", "OK");
            } else {
                req->send(400, "text/plain", "Invalid pin");
            }
        });

    server.on("/rules", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", rulesJson());
    });

    server.on("/restart", HTTP_POST, [](AsyncWebServerRequest* req) {
        req->send(200, "text/plain", "Restarting...");
        delay(300); ESP.restart();
    });

    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    server.begin();
    Serial.println("HTTP server started");
}

// ─── Loop: delayed transitions + rule evaluation ──────────────────────────────
void loop() {
    uint32_t now = millis();

    // Fire any pending delayed state changes
    for (auto& p : gpioPins) {
        if (p.pendingActive && now >= p.pendingAt) {
            digitalWrite(p.pin, p.pendingState);
            p.pendingActive = false;
        }
    }

    // Evaluate automation rules every 100 ms
    static uint32_t lastRuleMs = 0;
    if (now - lastRuleMs >= 100) {
        lastRuleMs = now;
        evaluateRules();
    }
}
