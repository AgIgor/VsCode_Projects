#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Configurações WiFi (AP) - Altere para suas credenciais
const char* ssid = "ESP32-Luzes";
const char* password = "12345678";

// Pinos de entrada (pull-up)
#define PIN_TRAVA      25
#define PIN_DESTRAVA   26
#define PIN_PORTA      14
#define PIN_IGNICAO    13

// Pinos de saída
#define PIN_LUZ_INTERNA  18
#define PIN_LUZ_ASSOALHO 19
#define PIN_FAROL        16
#define PIN_DRL          17

// Servidor e WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Estados das entradas e saídas
struct IOState {
  bool trava;
  bool destrava;
  bool porta;
  bool ignicao;
  bool luzInterna;
  bool luzAssoalho;
  bool farol;
  bool drl;
} ioState;

IOState lastState;
bool lastStateValid = false;

// Estrutura de regra
struct Condition {
  String input;
  bool state;
  String logicOp;  // "AND", "OR", ou vazio
};

struct Action {
  String output;
  bool state;
  int delay;
  int duration;
};

struct ActionEntry {
  Action action;
  unsigned long delayTimer;
  unsigned long durationTimer;
};

struct Rule {
  String type;  // "se", "quando", "enquanto"
  std::vector<Condition> conditions;
  std::vector<ActionEntry> actions;
  std::vector<ActionEntry> elseActions;
  bool hasElse;       // Indica se tem "senao"
  String original;
  bool active;
};

std::vector<Rule> rules;

// Detectores de pulso
unsigned long lastTravaTime = 0;
unsigned long lastDestravaTime = 0;
const int PULSE_COOLDOWN = 500; // ms

// Forward declarations
void sendIOState();
void applyOutput(String output, bool state);
void evaluateRules();
Rule parseRule(String line);
bool hasIOStateChanged();
void updateLastState();
void sendIOStateIfChanged();

// Função para ler entradas
void readInputs() {
  ioState.porta = !digitalRead(PIN_PORTA);        // Invertido por pull-up
  ioState.ignicao = !digitalRead(PIN_IGNICAO);    // Invertido por pull-up
  
  // Detecta pulsos de trava/destrava
  if (!digitalRead(PIN_TRAVA) && (millis() - lastTravaTime > PULSE_COOLDOWN)) {
    ioState.trava = true;
    lastTravaTime = millis();
    Serial.println("Pulso TRAVA detectado");
  } else {
    ioState.trava = false;
  }
  
  if (!digitalRead(PIN_DESTRAVA) && (millis() - lastDestravaTime > PULSE_COOLDOWN)) {
    ioState.destrava = true;
    lastDestravaTime = millis();
    Serial.println("Pulso DESTRAVA detectado");
  } else {
    ioState.destrava = false;
  }
}

// Função para aplicar saídas
void applyOutput(String output, bool state) {
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
  
  Serial.printf("Saída %s: %s\n", output.c_str(), state ? "ON" : "OFF");
  
  // Notifica clientes WebSocket
  sendIOState();
  updateLastState();
}

// Avalia uma condição
bool evaluateCondition(const Condition& cond) {
  if (cond.input == "trava") return ioState.trava == cond.state;
  if (cond.input == "destrava") return ioState.destrava == cond.state;
  if (cond.input == "porta") return ioState.porta == cond.state;
  if (cond.input == "ignicao") return ioState.ignicao == cond.state;
  // Suporte a saídas como condições
  if (cond.input == "luz-interna") return ioState.luzInterna == cond.state;
  if (cond.input == "luz-assoalho") return ioState.luzAssoalho == cond.state;
  if (cond.input == "farol") return ioState.farol == cond.state;
  if (cond.input == "drl") return ioState.drl == cond.state;
  return false;
}

// Avalia todas as condições de uma regra
bool evaluateConditions(const std::vector<Condition>& conditions) {
  if (conditions.empty()) return false;
  
  bool result = evaluateCondition(conditions[0]);
  
  for (size_t i = 1; i < conditions.size(); i++) {
    bool condResult = evaluateCondition(conditions[i]);
    String prevOp = conditions[i-1].logicOp;
    
    if (prevOp == "AND") {
      result = result && condResult;
    } else if (prevOp == "OR") {
      result = result || condResult;
    }
  }
  
  return result;
}

// Executa uma ação com timers
void executeActionEntry(ActionEntry& entry) {
  unsigned long now = millis();
  
  if (entry.action.delay > 0 && entry.delayTimer == 0 && entry.durationTimer == 0) {
    entry.delayTimer = now + (entry.action.delay * 1000);
    return;
  }
  
  if (entry.delayTimer > 0 && now < entry.delayTimer) {
    return; // Aguardando delay
  }
  
  if (entry.delayTimer > 0 && now >= entry.delayTimer) {
    applyOutput(entry.action.output, entry.action.state);
    entry.delayTimer = 0;
    
    if (entry.action.duration > 0) {
      entry.durationTimer = now + (entry.action.duration * 1000);
    }
    return;
  }
  
  if (entry.action.delay == 0) {
    applyOutput(entry.action.output, entry.action.state);
    
    if (entry.action.duration > 0 && entry.durationTimer == 0) {
      entry.durationTimer = now + (entry.action.duration * 1000);
    }
  }
}

void executeActions(std::vector<ActionEntry>& actions) {
  for (auto& entry : actions) {
    executeActionEntry(entry);
  }
}

void resetActionDelays(std::vector<ActionEntry>& actions) {
  for (auto& entry : actions) {
    entry.delayTimer = 0;
  }
}

// Processa timers de duração
void processTimers() {
  unsigned long now = millis();
  
  for (auto& rule : rules) {
    for (auto& entry : rule.actions) {
      if (entry.durationTimer > 0 && now >= entry.durationTimer) {
        applyOutput(entry.action.output, !entry.action.state);
        entry.durationTimer = 0;
      }
    }
    
    for (auto& entry : rule.elseActions) {
      if (entry.durationTimer > 0 && now >= entry.durationTimer) {
        applyOutput(entry.action.output, !entry.action.state);
        entry.durationTimer = 0;
      }
    }
  }
}

// Avalia todas as regras
void evaluateRules() {
  for (auto& rule : rules) {
    if (evaluateConditions(rule.conditions)) {
      if (rule.type == "enquanto") {
        executeActions(rule.actions);
      } else if (!rule.active) {
        executeActions(rule.actions);
        rule.active = true;
      }
    } else {
      // Executa "senao" se existir
      if (rule.hasElse && rule.active) {
        executeActions(rule.elseActions);
      }
      
      // Reseta regras do tipo "se" e "quando"
      if (rule.type != "enquanto" && rule.active) {
        rule.active = false;
        resetActionDelays(rule.actions);
      }
    }
  }
}

// Parser de regra em texto
Rule parseRule(String line) {
  line.trim();
  line.toLowerCase();
  
  Rule rule;
  rule.active = false;
  rule.hasElse = false;
  rule.original = line;
  
  // Detecta tipo
  if (line.startsWith("quando")) rule.type = "quando";
  else if (line.startsWith("enquanto")) rule.type = "enquanto";
  else rule.type = "se";
  
  // Divide por "entao" e "senao"
  int entaoPos = line.indexOf("entao");
  if (entaoPos < 0) entaoPos = line.indexOf("então");
  if (entaoPos < 0) return rule;
  
  String condPart = line.substring(0, entaoPos);
  String restPart = line.substring(entaoPos + 5);
  
  // Verifica se tem "senao"
  int senaoPos = restPart.indexOf(" senao ");
  if (senaoPos < 0) senaoPos = restPart.indexOf(" senão ");
  
  String actionPart;
  String elseActionPart;
  
  if (senaoPos >= 0) {
    rule.hasElse = true;
    actionPart = restPart.substring(0, senaoPos);
    elseActionPart = restPart.substring(senaoPos + 7);
  } else {
    actionPart = restPart;
  }
  
  // Remove tipo da condição
  condPart.replace("se ", "");
  condPart.replace("quando ", "");
  condPart.replace("enquanto ", "");
  condPart.trim();
  
  // Parse condições (simples: apenas uma condição por enquanto)
  // Formato: "entrada == estado [and/or entrada == estado]"
  int eqPos = condPart.indexOf("==");
  if (eqPos > 0) {
    Condition cond;
    cond.input = condPart.substring(0, eqPos);
    cond.input.trim();
    
    String stateStr = condPart.substring(eqPos + 2);
    
    // Detecta operador lógico
    int andPos = stateStr.indexOf(" and ");
    int orPos = stateStr.indexOf(" or ");
    int ePos = stateStr.indexOf(" e ");
    int ouPos = stateStr.indexOf(" ou ");
    
    if (andPos > 0 || ePos > 0) {
      cond.logicOp = "AND";
      int splitPos = andPos > 0 ? andPos : ePos;
      stateStr = stateStr.substring(0, splitPos);
    } else if (orPos > 0 || ouPos > 0) {
      cond.logicOp = "OR";
      int splitPos = orPos > 0 ? orPos : ouPos;
      stateStr = stateStr.substring(0, splitPos);
    }
    
    stateStr.trim();
    bool validState = false;
    if (stateStr == "on" || stateStr == "liga") {
      cond.state = true;
      validState = true;
    } else if (stateStr == "off" || stateStr == "desliga") {
      cond.state = false;
      validState = true;
    } else if (cond.input == "porta") {
      if (stateStr == "aberta") {
        cond.state = true;
        validState = true;
      } else if (stateStr == "fechada") {
        cond.state = false;
        validState = true;
      }
    }

    if (validState) {
      rule.conditions.push_back(cond);
    }
  }
  
  auto parseActionSegment = [](String text, Action& action) -> bool {
    text.trim();
    int eqPos = text.indexOf("==");
    int eqLen = 2;
    if (eqPos <= 0) {
      eqPos = text.indexOf("=");
      eqLen = 1;
    }
    if (eqPos <= 0) return false;
    
    action.output = text.substring(0, eqPos);
    action.output.trim();
    
    String actionState = text.substring(eqPos + eqLen);
    actionState.trim();
    
    int emPos = actionState.indexOf(" em ");
    if (emPos > 0) {
      String delayStr = actionState.substring(emPos + 4);
      int sPos = delayStr.indexOf("s");
      if (sPos > 0) {
        action.delay = delayStr.substring(0, sPos).toInt();
      }
      actionState = actionState.substring(0, emPos);
    }
    
    int porPos = actionState.indexOf(" por ");
    if (porPos > 0) {
      String durStr = actionState.substring(porPos + 5);
      int sPos = durStr.indexOf("s");
      if (sPos > 0) {
        action.duration = durStr.substring(0, sPos).toInt();
      }
      actionState = actionState.substring(0, porPos);
    }
    
    actionState.trim();
    if (actionState == "on" || actionState == "liga") {
      action.state = true;
      return true;
    }
    if (actionState == "off" || actionState == "desliga") {
      action.state = false;
      return true;
    }
    return false;
  };
  
  auto parseActionList = [&](String text, std::vector<ActionEntry>& target) {
    text.trim();
    while (text.length() > 0) {
      int ePos = text.indexOf(" e ");
      int andPos = text.indexOf(" and ");
      int splitPos = -1;
      int splitLen = 0;
      
      if (ePos >= 0 && (andPos < 0 || ePos < andPos)) {
        splitPos = ePos;
        splitLen = 3;
      } else if (andPos >= 0) {
        splitPos = andPos;
        splitLen = 5;
      }
      
      String part;
      if (splitPos >= 0) {
        part = text.substring(0, splitPos);
        text = text.substring(splitPos + splitLen);
      } else {
        part = text;
        text = "";
      }
      
      Action action = {};
      if (parseActionSegment(part, action)) {
        ActionEntry entry;
        entry.action = action;
        entry.delayTimer = 0;
        entry.durationTimer = 0;
        target.push_back(entry);
      }
    }
  };
  
  // Parse ações
  parseActionList(actionPart, rule.actions);
  
  // Parse ação do "senao" se existir
  if (rule.hasElse) {
    parseActionList(elseActionPart, rule.elseActions);
  }
  
  return rule;
}

// Envia estado das I/O via WebSocket
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

bool hasIOStateChanged() {
  if (!lastStateValid) return true;
  return ioState.trava != lastState.trava ||
         ioState.destrava != lastState.destrava ||
         ioState.porta != lastState.porta ||
         ioState.ignicao != lastState.ignicao ||
         ioState.luzInterna != lastState.luzInterna ||
         ioState.luzAssoalho != lastState.luzAssoalho ||
         ioState.farol != lastState.farol ||
         ioState.drl != lastState.drl;
}

void updateLastState() {
  lastState = ioState;
  lastStateValid = true;
}

void sendIOStateIfChanged() {
  if (hasIOStateChanged()) {
    sendIOState();
    updateLastState();
  }
}

// Callbacks WebSocket
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("Cliente WebSocket #%u conectado\n", client->id());
    // Envia estado atual
    sendIOState();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("Cliente WebSocket #%u desconectado\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
      data[len] = 0;
      String msg = (char*)data;
      
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, msg);
      
      if (!error) {
        if (doc.containsKey("rules")) {
          // Recebe novas regras
          rules.clear();
          String rulesText = doc["rules"].as<String>();
          
          int start = 0;
          int end = rulesText.indexOf('\n');
          
          while (end >= 0) {
            String line = rulesText.substring(start, end);
            line.trim();
            if (line.length() > 0 && !line.startsWith("//")) {
              Rule rule = parseRule(line);
              if (!rule.conditions.empty()) {
                rules.push_back(rule);
                Serial.println("Regra adicionada: " + line);
              }
            }
            start = end + 1;
            end = rulesText.indexOf('\n', start);
          }
          
          // Última linha
          String line = rulesText.substring(start);
          line.trim();
          if (line.length() > 0 && !line.startsWith("//")) {
            Rule rule = parseRule(line);
            if (!rule.conditions.empty()) {
              rules.push_back(rule);
              Serial.println("Regra adicionada: " + line);
            }
          }
          
          Serial.printf("Total de %d regra(s) carregadas\n", rules.size());
        }
        
        // Controle manual de saídas
        if (doc.containsKey("output")) {
          String output = doc["output"].as<String>();
          bool state = doc["state"].as<bool>();
          applyOutput(output, state);
          Serial.printf("Controle manual: %s = %s\n", output.c_str(), state ? "ON" : "OFF");
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== Sistema de Controle de Luzes ===\n");
  
  // Configura pinos de entrada (pull-up)
  pinMode(PIN_TRAVA, INPUT_PULLUP);
  pinMode(PIN_DESTRAVA, INPUT_PULLUP);
  pinMode(PIN_PORTA, INPUT_PULLUP);
  pinMode(PIN_IGNICAO, INPUT_PULLUP);
  
  // Configura pinos de saída
  pinMode(PIN_LUZ_INTERNA, OUTPUT);
  pinMode(PIN_LUZ_ASSOALHO, OUTPUT);
  pinMode(PIN_FAROL, OUTPUT);
  pinMode(PIN_DRL, OUTPUT);
  
  // Inicializa saídas em LOW
  digitalWrite(PIN_LUZ_INTERNA, LOW);
  digitalWrite(PIN_LUZ_ASSOALHO, LOW);
  digitalWrite(PIN_FAROL, LOW);
  digitalWrite(PIN_DRL, LOW);
  
  // Inicializa LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Erro ao montar LittleFS!");
    return;
  }
  Serial.println("LittleFS montado com sucesso");
  
  // Inicia WiFi em modo AP
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.softAP(ssid, password);
  Serial.println("WiFi AP iniciado!");
  Serial.print("AP SSID: ");
  Serial.println(ssid);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Configura WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  // Serve arquivos estáticos
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  // Rota para obter estado
  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request){
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
  Serial.println("Servidor HTTP iniciado!");
  Serial.println("\nAcesse: http://" + WiFi.softAPIP().toString());
}

unsigned long lastCheck = 0;
const int CHECK_INTERVAL = 100; // ms

void loop() {
  ws.cleanupClients();
  
  unsigned long now = millis();
  if (now - lastCheck >= CHECK_INTERVAL) {
    lastCheck = now;
    
    // Lê entradas
    readInputs();
    sendIOStateIfChanged();
    
    // Avalia regras
    evaluateRules();
    
    // Processa timers
    processTimers();
  }
  
  delay(10);
}