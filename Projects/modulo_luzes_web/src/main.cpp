#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// WiFi Configuration
const char* ssid = "ESP32-Luzes";
const char* password = "12345678";

// Input pins (pull-up)
#define PIN_TRAVA      25
#define PIN_DESTRAVA   26
#define PIN_PORTA      14
#define PIN_IGNICAO    13

// Output pins
#define PIN_LUZ_INTERNA  18
#define PIN_LUZ_ASSOALHO 19
#define PIN_FAROL        16
#define PIN_DRL          17

// WebServer and WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// IO States
struct IOState {
  bool trava;
  bool destrava;
  bool porta;
  bool ignicao;
  bool luzInterna;
  bool luzAssoalho;
  bool farol;
  bool drl;
};

IOState ioState = {};
IOState prevState = {};
bool prevStateValid = false;

// Rule Structures
struct Condition {
  String input;        // "trava", "porta", "ignicao", etc.
  String stateType;    // "ligou", "desligou", "abriu", "fechou", "ligado", "desligado", "aberto", "fechado"
  String logicOp;      // "AND", "OR"
};

struct Action {
  String output;       // "farol", "luz-interna", etc.
  String actionType;   // "liga", "desliga"
  int delayMs;         // delay before execution
  int durationMs;      // how long to keep it on
  unsigned long delayTimer = 0;
  unsigned long durationTimer = 0;
};

struct Rule {
  std::vector<Condition> conditions;
  std::vector<Action> actions;
  String original;
};

std::vector<Rule> rules;
const char* RULES_FILE = "/rules.txt";
const size_t MAX_RULES_TEXT = 4096;
const size_t MAX_RULES = 40;
const size_t MAX_CONDITIONS = 12;
const size_t MAX_ACTIONS = 12;

// Pulse detection
unsigned long lastTravaTime = 0;
unsigned long lastDestravaTime = 0;
const int PULSE_COOLDOWN = 500;

// Forward declarations
void readInputs();
void sendIOState();
void applyOutput(const String& output, bool state);
void evaluateRules();
void processTimers();
void updatePrevState();
void applyRulesText(const String& rulesText);
bool saveRulesToFS(const String& rulesText);
bool loadRulesFromFS();
String normalizeStateType(const String& stateType);

// Get current IO value
bool getIOValue(const IOState& state, const String& input, bool& value) {
  if (input == "trava") { value = state.trava; return true; }
  if (input == "destrava") { value = state.destrava; return true; }
  if (input == "porta") { value = state.porta; return true; }
  if (input == "ignicao") { value = state.ignicao; return true; }
  if (input == "luz-interna") { value = state.luzInterna; return true; }
  if (input == "luz-assoalho") { value = state.luzAssoalho; return true; }
  if (input == "farol") { value = state.farol; return true; }
  if (input == "drl") { value = state.drl; return true; }
  return false;
}

// Set IO value
void setIOValue(IOState& state, const String& input, bool value) {
  if (input == "trava") { state.trava = value; return; }
  if (input == "destrava") { state.destrava = value; return; }
  if (input == "porta") { state.porta = value; return; }
  if (input == "ignicao") { state.ignicao = value; return; }
  if (input == "luz-interna") { state.luzInterna = value; return; }
  if (input == "luz-assoalho") { state.luzAssoalho = value; return; }
  if (input == "farol") { state.farol = value; return; }
  if (input == "drl") { state.drl = value; return; }
}

String normalizeStateType(const String& stateType) {
  if (stateType == "ligada") return "ligado";
  if (stateType == "desligada") return "desligado";
  if (stateType == "aberta") return "aberto";
  if (stateType == "fechada") return "fechado";
  return stateType;
}

// Evaluate a single condition
bool evaluateCondition(const Condition& cond) {
  String stateType = normalizeStateType(cond.stateType);
  bool current = false;
  if (!getIOValue(ioState, cond.input, current)) {
    Serial.printf("Error: Unknown input '%s'\n", cond.input.c_str());
    return false;
  }

  bool prev = false;
  if (!prevStateValid) {
    // Can't evaluate edge conditions on first run
    if (stateType == "ligou" || stateType == "desligou" || 
        stateType == "abriu" || stateType == "fechou") {
      return false;
    }
  } else {
    getIOValue(prevState, cond.input, prev);
  }

  // Edge triggers (state changes)
  if (stateType == "ligou") {
    return !prev && current;  // false to true
  }
  if (stateType == "desligou") {
    return prev && !current;  // true to false
  }
  if (stateType == "abriu") {
    return !prev && current;  // false to true (porta opens)
  }
  if (stateType == "fechou") {
    return prev && !current;  // true to false (porta closes)
  }

  // State checks
  if (stateType == "ligado") {
    return current == true;
  }
  if (stateType == "desligado") {
    return current == false;
  }
  if (stateType == "aberto") {
    return current == true;   // porta aberto = true
  }
  if (stateType == "fechado") {
    return current == false;  // porta fechado = false
  }

  return false;
}

// Evaluate all conditions for a rule
bool evaluateAllConditions(const std::vector<Condition>& conditions) {
  if (conditions.empty()) return false;

  bool result = evaluateCondition(conditions[0]);

  for (size_t i = 1; i < conditions.size(); i++) {
    bool condResult = evaluateCondition(conditions[i]);
    const String& op = conditions[i].logicOp;

    if (op == "AND") {
      result = result && condResult;
    } else if (op == "OR") {
      result = result || condResult;
    }
  }

  return result;
}

// Execute action
void applyOutput(const String& output, bool state) {
  if (output == "luz-interna") {
    digitalWrite(PIN_LUZ_INTERNA, state);
    ioState.luzInterna = state;
  } else if (output == "luz-assoalho") {
    digitalWrite(PIN_LUZ_ASSOALHO, state);
    ioState.luzAssoalho = state;
  } else if (output == "farol") {
    digitalWrite(PIN_FAROL, state);
    ioState.farol = state;
  } else if (output == "drl") {
    digitalWrite(PIN_DRL, state);
    ioState.drl = state;
  }

  Serial.printf("Output %s: %s\n", output.c_str(), state ? "ON" : "OFF");
  sendIOState();
}

// Process action with timers
void executeAction(Action& action) {
  unsigned long now = millis();

  // Handle delay
  if (action.delayMs > 0 && action.delayTimer == 0) {
    action.delayTimer = now + action.delayMs;
    return;  // Wait for delay
  }

  // Still waiting for delay
  if (action.delayTimer > 0 && now < action.delayTimer) {
    return;
  }

  // Delay complete, apply action
  if (action.delayTimer > 0 && now >= action.delayTimer) {
    bool state = (action.actionType == "liga");
    applyOutput(action.output, state);
    action.delayTimer = 0;

    // Set up duration timer
    if (action.durationMs > 0) {
      action.durationTimer = now + action.durationMs;
    }
    return;
  }

  // No delay, apply immediately
  if (action.delayMs == 0 && action.delayTimer == 0) {
    bool state = (action.actionType == "liga");
    applyOutput(action.output, state);

    // Set up duration timer
    if (action.durationMs > 0 && action.durationTimer == 0) {
      action.durationTimer = now + action.durationMs;
    }
  }
}

// Process all timers
void processTimers() {
  unsigned long now = millis();

  for (auto& rule : rules) {
    for (auto& action : rule.actions) {
      // Duration timer expired, turn off
      if (action.durationTimer > 0 && now >= action.durationTimer) {
        bool state = (action.actionType == "liga");
        applyOutput(action.output, !state);  // Turn off (opposite of action)
        action.durationTimer = 0;
      }
    }
  }
}

// Evaluate all rules
void evaluateRules() {
  for (auto& rule : rules) {
    if (evaluateAllConditions(rule.conditions)) {
      Serial.println("Rule matched!");
      for (auto& action : rule.actions) {
        executeAction(action);
      }
    }
  }
}

// Parse rule from text
Rule parseRule(const String& line) {
  Rule rule;
  rule.original = line;

  String text = line;
  text.toLowerCase();
  text.trim();

  // Find "entao" or "então"
  int entaoPos = text.indexOf("entao");
  if (entaoPos < 0) entaoPos = text.indexOf("então");
  if (entaoPos < 0) {
    Serial.println("Error: 'entao' not found in rule");
    return rule;
  }

  String condPart = text.substring(0, entaoPos);
  String actionPart = text.substring(entaoPos + 5);

  // Remove "se " from beginning
  if (condPart.startsWith("se ")) {
    condPart = condPart.substring(3);
  }
  condPart.trim();
  actionPart.trim();

  // Parse conditions (split by " e " and " ou ")
  int idx = 0;
  while (idx < condPart.length()) {
    // Find next operator
    int ePos = condPart.indexOf(" e ", idx);
    int ouPos = condPart.indexOf(" ou ", idx);
    int nextOpPos = -1;
    String nextOp;

    if (ePos >= idx && ouPos >= idx) {
      if (ePos < ouPos) {
        nextOpPos = ePos;
        nextOp = "AND";
      } else {
        nextOpPos = ouPos;
        nextOp = "OR";
      }
    } else if (ePos >= idx) {
      nextOpPos = ePos;
      nextOp = "AND";
    } else if (ouPos >= idx) {
      nextOpPos = ouPos;
      nextOp = "OR";
    }

    String segment;
    String nextOpStr;
    if (nextOpPos >= 0) {
      segment = condPart.substring(idx, nextOpPos);
      nextOpStr = nextOp;
      idx = nextOpPos + (nextOp == "AND" ? 3 : 4);
    } else {
      segment = condPart.substring(idx);
    }

    segment.trim();
    if (segment.length() > 0) {
      // Parse condition: "input stateType"
      int spacePos = segment.lastIndexOf(' ');
      if (spacePos > 0) {
        String input = segment.substring(0, spacePos);
        input.trim();
        String stateType = segment.substring(spacePos + 1);
        stateType.trim();
        stateType = normalizeStateType(stateType);

        if (rule.conditions.size() < MAX_CONDITIONS) {
          Condition cond;
          cond.input = input;
          cond.stateType = stateType;
          cond.logicOp = nextOpStr;
          rule.conditions.push_back(cond);
        }
      }
    }
  }

  // Parse actions (split by " e ")
  idx = 0;
  while (idx < actionPart.length()) {
    int ePos = actionPart.indexOf(" e ", idx);
    String segment;
    if (ePos >= idx) {
      segment = actionPart.substring(idx, ePos);
      idx = ePos + 3;
    } else {
      segment = actionPart.substring(idx);
      idx = actionPart.length();
    }

    segment.trim();
    if (segment.length() > 0) {
      // Parse action: "actionType output [em Xs] [por Ys]"
      // Example: "liga farol em 1s por 5s"

      Action action = {};

      // Parse timing
      int emPos = segment.indexOf(" em ");
      int porPos = segment.indexOf(" por ");

      if (emPos >= 0) {
        String delayStr = segment.substring(emPos + 4);
        int sPos = delayStr.indexOf("s");
        if (sPos > 0) {
          action.delayMs = delayStr.substring(0, sPos).toInt() * 1000;
        }
        segment = segment.substring(0, emPos);
      }

      if (porPos >= 0) {
        porPos = segment.indexOf(" por ");  // Check again after removing " em " part
        if (porPos >= 0) {
          String durStr = segment.substring(porPos + 5);
          int sPos = durStr.indexOf("s");
          if (sPos > 0) {
            action.durationMs = durStr.substring(0, sPos).toInt() * 1000;
          }
          segment = segment.substring(0, porPos);
        }
      }

      segment.trim();

      // Parse "actionType output"
      int spacePos = segment.indexOf(' ');
      if (spacePos > 0 && rule.actions.size() < MAX_ACTIONS) {
        String actionType = segment.substring(0, spacePos);
        String output = segment.substring(spacePos + 1);
        action.actionType = actionType;
        action.output = output;

        rule.actions.push_back(action);
      }
    }
  }

  return rule;
}

void applyRulesText(const String& rulesText) {
  if (rulesText.length() > MAX_RULES_TEXT) {
    Serial.println("Error: Rules text too large, ignoring");
    return;
  }

  rules.clear();
  rules.reserve(MAX_RULES);

  int start = 0;
  while (start < rulesText.length()) {
    int end = rulesText.indexOf('\n', start);
    if (end < 0) end = rulesText.length();

    String line = rulesText.substring(start, end);
    line.trim();

    if (line.length() > 0 && !line.startsWith("//")) {
      if (rules.size() >= MAX_RULES) {
        Serial.println("Warning: Max rules reached, ignoring extra");
        break;
      }
      Rule rule = parseRule(line);
      if (!rule.conditions.empty() && !rule.actions.empty()) {
        rules.push_back(rule);
        Serial.println("Rule loaded: " + line);
      } else {
        Serial.println("Rule ignored (invalid): " + line);
      }
    }

    start = end + 1;
  }

  Serial.printf("Total rules: %d\n", rules.size());
}

bool saveRulesToFS(const String& rulesText) {
  File file = LittleFS.open(RULES_FILE, "w");
  if (!file) {
    Serial.println("Error: Failed to open rules file for writing");
    return false;
  }
  file.print(rulesText);
  file.close();
  return true;
}

bool loadRulesFromFS() {
  if (!LittleFS.exists(RULES_FILE)) {
    Serial.println("No saved rules found");
    return false;
  }

  File file = LittleFS.open(RULES_FILE, "r");
  if (!file) {
    Serial.println("Error: Failed to open rules file for reading");
    return false;
  }

  String rulesText = file.readString();
  file.close();

  applyRulesText(rulesText);
  return true;
}

// Send IO state via WebSocket
void sendIOState() {
  StaticJsonDocument<256> doc;
  doc["trava"] = ioState.trava;
  doc["destrava"] = ioState.destrava;
  doc["porta"] = ioState.porta;
  doc["ignicao"] = ioState.ignicao;
  doc["luzInterna"] = ioState.luzInterna;
  doc["luzAssoalho"] = ioState.luzAssoalho;
  doc["farol"] = ioState.farol;
  doc["drl"] = ioState.drl;

  String json;
  serializeJson(doc, json);
  ws.textAll(json);
}

// Read input pins
void readInputs() {
  ioState.porta = !digitalRead(PIN_PORTA);
  ioState.ignicao = !digitalRead(PIN_IGNICAO);

  // Pulse detection for trava/destrava
  if (!digitalRead(PIN_TRAVA) && (millis() - lastTravaTime > PULSE_COOLDOWN)) {
    ioState.trava = true;
    lastTravaTime = millis();
    Serial.println("TRAVA pulse detected");
  } else {
    ioState.trava = false;
  }

  if (!digitalRead(PIN_DESTRAVA) && (millis() - lastDestravaTime > PULSE_COOLDOWN)) {
    ioState.destrava = true;
    lastDestravaTime = millis();
    Serial.println("DESTRAVA pulse detected");
  } else {
    ioState.destrava = false;
  }
}

// Update previous state for edge detection
void updatePrevState() {
  prevState = ioState;
  prevStateValid = true;
}

// WebSocket event handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected\n", client->id());
    sendIOState();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
      String msg((char*)data, len);

      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, msg);

      if (!error) {
        if (doc.containsKey("rules")) {
          String rulesText = doc["rules"].as<String>();
          applyRulesText(rulesText);
          if (!saveRulesToFS(rulesText)) {
            Serial.println("Error: Failed to save rules");
          }
        }

        // Manual output control
        if (doc.containsKey("output")) {
          String output = doc["output"].as<String>();
          bool state = doc["state"].as<bool>();
          applyOutput(output, state);
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== Car Lights Control System ===\n");

  // Configure input pins (pull-up)
  pinMode(PIN_TRAVA, INPUT_PULLUP);
  pinMode(PIN_DESTRAVA, INPUT_PULLUP);
  pinMode(PIN_PORTA, INPUT_PULLUP);
  pinMode(PIN_IGNICAO, INPUT_PULLUP);

  // Configure output pins
  pinMode(PIN_LUZ_INTERNA, OUTPUT);
  pinMode(PIN_LUZ_ASSOALHO, OUTPUT);
  pinMode(PIN_FAROL, OUTPUT);
  pinMode(PIN_DRL, OUTPUT);

  // Initialize outputs to LOW
  digitalWrite(PIN_LUZ_INTERNA, LOW);
  digitalWrite(PIN_LUZ_ASSOALHO, LOW);
  digitalWrite(PIN_FAROL, LOW);
  digitalWrite(PIN_DRL, LOW);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
    return;
  }
  Serial.println("LittleFS mounted");

  if (loadRulesFromFS()) {
    Serial.println("Saved rules applied");
  }

  // Start WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.softAP(ssid, password);
  Serial.println("WiFi AP started");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // Configure WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Avoid filesystem errors for missing favicon files
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });
  server.on("/favicon.ico.gz", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });
  server.on("/favicon.ico/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });
  server.on("/favicon.ico/index.html.gz", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });
  server.on("/index.html.gz", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });

  // Serve static files
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // API endpoint for state
  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;
    doc["trava"] = ioState.trava;
    doc["destrava"] = ioState.destrava;
    doc["porta"] = ioState.porta;
    doc["ignicao"] = ioState.ignicao;
    doc["luzInterna"] = ioState.luzInterna;
    doc["luzAssoalho"] = ioState.luzAssoalho;
    doc["farol"] = ioState.farol;
    doc["drl"] = ioState.drl;

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Access: http://" + WiFi.softAPIP().toString());
}

unsigned long lastCheck = 0;
const int CHECK_INTERVAL = 100;  // ms

void loop() {
  ws.cleanupClients();

  unsigned long now = millis();
  if (now - lastCheck >= CHECK_INTERVAL) {
    lastCheck = now;

    // Read inputs
    readInputs();
    sendIOState();

    // Evaluate rules
    evaluateRules();

    // Process timers
    processTimers();

    // Update previous state for next edge detection
    updatePrevState();
  }

  delay(10);
}