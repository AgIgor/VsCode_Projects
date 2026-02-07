#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ============= CONFIGURAÇÕES =============
#define AP_PASSWORD "12345678"
#define AUTH_USER "admin"
#define AUTH_PASS "admin123"
#define DEFAULT_SSID "BCM_ESP32"
#define DEFAULT_BCM_NAME "BCM_ESP32"

// ENTRADAS (Pull-up ativo em LOW)
#define PIN_IGNICAO 13
#define PIN_PORTA 14
#define PIN_TRAVA 25
#define PIN_DESTRAVA 26

// SAÍDAS
#define PIN_FAROL 16
#define PIN_DRL 17
#define PIN_INTERNA 18
#define PIN_PES 19

// FAIL-SAFE
#define TIMEOUT_IGNICAO_OFF 600000  // 600 segundos (10 minutos) em ms

// ============= ESTRUTURAS =============
struct OutputState {
  bool active = false;
  unsigned long ligarTime = 0;
  unsigned long desligarTime = 0;
};

struct InputState {
  bool current = HIGH;
  bool previous = HIGH;
  unsigned long lastChange = 0;
  const uint32_t debounce = 50;
};

// ============= VARIÁVEIS GLOBAIS =============
AsyncWebServer server(80);
JsonDocument configDoc;

// WiFi e BCM configuráveis
String wifiSSID = DEFAULT_SSID;
String bcmName = DEFAULT_BCM_NAME;

OutputState outputStates[4]; // farol, drl, interna, pes
InputState inputStates[4];   // ignicao, porta, trava, destrava

const char* outputNames[] = {"farol", "drl", "interna", "pes"};
const uint8_t outputPins[] = {PIN_FAROL, PIN_DRL, PIN_INTERNA, PIN_PES};
const uint8_t inputPins[] = {PIN_IGNICAO, PIN_PORTA, PIN_DESTRAVA, PIN_TRAVA};

// Fail-safe: controle de timeout
unsigned long ignicaoOffTime = 0;  // Momento em que ignição foi desligada
bool ignicaoOffTimeoutActive = false;
bool failsafeTriggered = false;

// WiFi sob demanda
int ignicaoToggleCount = 0;
unsigned long lastIgnicaoToggle = 0;
const unsigned long IGNICAO_TOGGLE_WINDOW = 10000; // 10 segundos para fazer 5 ligadas
bool wifiEnabled = true; // Inicia ligado na primeira vez

bool checkAuth(AsyncWebServerRequest *request) {
  if (request->authenticate(AUTH_USER, AUTH_PASS)) return true;
  request->requestAuthentication();
  return false;
}

// Buffer de logs para servir via HTTP
constexpr size_t LOG_CAPACITY = 120;
String logBuffer[LOG_CAPACITY];
size_t logHead = 0;
size_t logCount = 0;

void pushLog(const String& msg) {
  logBuffer[logHead] = msg;
  logHead = (logHead + 1) % LOG_CAPACITY;
  if (logCount < LOG_CAPACITY) logCount++;
  Serial.println(msg);
}

void pushLogf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  pushLog(String(buf));
}

// ============= FUNÇÕES AUXILIARES =============

void enableWiFi() {
  if (wifiEnabled) return;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(wifiSSID.c_str(), AP_PASSWORD);
  wifiEnabled = true;
  pushLog("✅ WiFi ATIVADO - SSID: " + wifiSSID + " - http://" + WiFi.softAPIP().toString());
}

void disableWiFi() {
  if (!wifiEnabled) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiEnabled = false;
  pushLog("🔴 WiFi DESLIGADO (economia de bateria)");
}

void shutdownAllOutputs() {
  pushLog("⚠️  FAIL-SAFE ATIVADO: Desligando todas as saídas!");
  for (int i = 0; i < 4; i++) {
    digitalWrite(outputPins[i], LOW);
    outputStates[i].active = false;
    outputStates[i].ligarTime = 0;
    outputStates[i].desligarTime = 0;
    pushLogf("  ✗ %s desligado", outputNames[i]);
  }
  disableWiFi(); // Desligar WiFi também
  failsafeTriggered = true;
}

void initFileSystem() {
  if (!LittleFS.begin(true)) {
    pushLog("Erro ao montar LittleFS");
    return;
  }
  pushLog("LittleFS montado com sucesso");
}

void loadConfig() {
  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    pushLog("Arquivo config.json não encontrado");
    return;
  }
  
  DeserializationError error = deserializeJson(configDoc, file);
  file.close();
  
  if (error) {
    pushLog(String("Erro ao ler config: ") + error.c_str());
    return;
  }

  // Carregar WiFi SSID e BCM Name das configurações
  if (configDoc["wifiSSID"].is<const char*>()) {
    wifiSSID = configDoc["wifiSSID"].as<const char*>();
  } else {
    wifiSSID = DEFAULT_SSID;
  }
  
  if (configDoc["bcmName"].is<const char*>()) {
    bcmName = configDoc["bcmName"].as<const char*>();
  } else {
    bcmName = DEFAULT_BCM_NAME;
  }

  String pretty;
  serializeJsonPretty(configDoc, pretty);
  pushLog("Configuração carregada:");
  pushLog(pretty);
}

void saveConfig(const String& jsonData) {
  File file = LittleFS.open("/config.json", "w");
  if (!file) {
    pushLog("Erro ao abrir config.json para escrita");
    return;
  }
  
  file.print(jsonData);
  file.close();
  pushLog("Configuração salva com sucesso");
}

bool checkCondition(JsonObject conditions) {
  // Verifica condição de porta
  const char* portaCond = conditions["porta"] | "any";
  if (strcmp(portaCond, "any") != 0) {
    bool portaOpen = digitalRead(PIN_PORTA) == LOW; // LOW = porta aberta (pull-up)
    if (strcmp(portaCond, "open") == 0 && !portaOpen) return false;
    if (strcmp(portaCond, "closed") == 0 && portaOpen) return false;
  }
  
  // Verifica condição de ignição
  const char* ignicaoCond = conditions["ignicao"] | "any";
  if (strcmp(ignicaoCond, "any") != 0) {
    bool ignicaoOn = digitalRead(PIN_IGNICAO) == LOW; // LOW = ignição ligada (pull-up)
    if (strcmp(ignicaoCond, "on") == 0 && !ignicaoOn) return false;
    if (strcmp(ignicaoCond, "off") == 0 && ignicaoOn) return false;
  }
  
  return true;
}

void processEvent(const char* eventName) {
  pushLog(String("Evento detectado: ") + eventName);
  
  if (!configDoc.containsKey("logicas")) return;
  
  JsonArray logicas = configDoc["logicas"].as<JsonArray>();
  
  for (JsonObject logica : logicas) {
    const char* evento = logica["evento"];
    if (strcmp(evento, eventName) != 0) continue;
    
    // Verifica condições
    if (!checkCondition(logica["conditions"].as<JsonObject>())) {
      pushLog("  Condições não atendidas");
      continue;
    }
    
    pushLog("  Condições atendidas, processando saídas:");
    JsonObject outputs = logica["outputs"].as<JsonObject>();
    
    for (int i = 0; i < 4; i++) {
      if (!outputs.containsKey(outputNames[i])) continue;
      
      JsonObject output = outputs[outputNames[i]];
      bool enabled = output["enabled"] | false;
      
      if (!enabled) continue;
      
      int ligarAfter = output["ligar"]["after"] | 0;
      int desligarAfter = output["desligar"]["after"] | 0;
      
      unsigned long now = millis();
      
      if (ligarAfter > 0) {
        outputStates[i].ligarTime = now + (ligarAfter * 1000);
        pushLogf("    %s: ligar após %ds", outputNames[i], ligarAfter);
      }
      
      if (desligarAfter > 0) {
        outputStates[i].desligarTime = now + (desligarAfter * 1000);
        pushLogf("    %s: desligar após %ds", outputNames[i], desligarAfter);
      }
    }
  }
}

void updateOutputs() {
  unsigned long now = millis();
  
  for (int i = 0; i < 4; i++) {
    // Verifica se deve ligar
    if (outputStates[i].ligarTime > 0 && now >= outputStates[i].ligarTime) {
      digitalWrite(outputPins[i], HIGH);
      outputStates[i].active = true;
      outputStates[i].ligarTime = 0;
      pushLogf("✓ %s LIGADO", outputNames[i]);
    }
    
    // Verifica se deve desligar
    if (outputStates[i].desligarTime > 0 && now >= outputStates[i].desligarTime) {
      digitalWrite(outputPins[i], LOW);
      outputStates[i].active = false;
      outputStates[i].desligarTime = 0;
      pushLogf("✗ %s DESLIGADO", outputNames[i]);
    }
  }
}

void handleEvent(const char* eventName) {
  if (strcmp(eventName, "ignicao_on") == 0) {
    processEvent("ignicao_on");
    ignicaoOffTimeoutActive = false;
    failsafeTriggered = false;
    pushLog("✓ Ignição ligada - fail-safe desativado");
    
    // Contador de ligadas para toggle WiFi
    unsigned long now = millis();
    if (now - lastIgnicaoToggle > IGNICAO_TOGGLE_WINDOW) {
      ignicaoToggleCount = 0; // Reset se passou muito tempo
    }
    ignicaoToggleCount++;
    lastIgnicaoToggle = now;
    
    if (ignicaoToggleCount >= 5) {
      if (wifiEnabled) {
        disableWiFi();
      } else {
        enableWiFi();
      }
      ignicaoToggleCount = 0;
    } else {
      pushLogf("🔑 Ignição ligada %d/5 (toggle WiFi)", ignicaoToggleCount);
    }
    return;
  }
  
  if (strcmp(eventName, "ignicao_off") == 0) {
    processEvent("ignicao_off");
    ignicaoOffTime = millis();
    ignicaoOffTimeoutActive = true;
    failsafeTriggered = false;
    pushLogf("⏱️  Ignição desligada - fail-safe ativo em %d segundos", TIMEOUT_IGNICAO_OFF/1000);
    return;
  }
  
  processEvent(eventName);
}

void checkInputs() {
  unsigned long now = millis();
  bool anyChange = false;
  
  bool evIgnicaoOn = false;
  bool evIgnicaoOff = false;
  bool evPortaAbriu = false;
  bool evPortaFechou = false;
  bool evTravou = false;
  bool evDestravou = false;
  
  for (int i = 0; i < 4; i++) {
    bool reading = digitalRead(inputPins[i]);
    
    // Debounce
    if (reading != inputStates[i].current) {
      if (now - inputStates[i].lastChange > inputStates[i].debounce) {
        inputStates[i].previous = inputStates[i].current;
        inputStates[i].current = reading;
        inputStates[i].lastChange = now;
        anyChange = true;
        
        // Detectar mudança de HIGH para LOW (botão pressionado com pull-up)
        if (inputStates[i].previous == HIGH && inputStates[i].current == LOW) {
          switch(i) {
            case 0: evIgnicaoOn = true; break;
            case 1: evPortaAbriu = true; break;
            case 2: evDestravou = true; break;
            case 3: evTravou = true; break;
          }
        }
        // Detectar mudança de LOW para HIGH (botão liberado)
        else if (inputStates[i].previous == LOW && inputStates[i].current == HIGH) {
          switch(i) {
            case 0: evIgnicaoOff = true; break;
            case 1: evPortaFechou = true; break;
          }
        }
      }
    }
  }
  
  if (!anyChange) return;
  
  // Intertravamento: ignicao_on tem prioridade sobre ignicao_off no mesmo ciclo
  if (evIgnicaoOn) evIgnicaoOff = false;
  
  // Intertravamento: travou tem prioridade sobre destravou no mesmo ciclo
  if (evTravou) evDestravou = false;
  
  // Prioridade: ignicao_on > ignicao_off > travou > destravou > porta_abriu > porta_fechou
  if (evIgnicaoOn) handleEvent("ignicao_on");
  if (evIgnicaoOff) handleEvent("ignicao_off");
  if (evTravou) handleEvent("travou");
  if (evDestravou) handleEvent("destravou");
  if (evPortaAbriu) handleEvent("porta_abriu");
  if (evPortaFechou) handleEvent("porta_fechou");
}

// ============= SETUP =============

void setup() {
  Serial.begin(115200);
  pushLog("=== ESP32 BCM Automotive ===");
  
  // Configurar entradas com pull-up
  pinMode(PIN_IGNICAO, INPUT_PULLUP);
  pinMode(PIN_PORTA, INPUT_PULLUP);
  pinMode(PIN_TRAVA, INPUT_PULLUP);
  pinMode(PIN_DESTRAVA, INPUT_PULLUP);
  
  // Configurar saídas
  pinMode(PIN_FAROL, OUTPUT);
  pinMode(PIN_DRL, OUTPUT);
  pinMode(PIN_INTERNA, OUTPUT);
  pinMode(PIN_PES, OUTPUT);
  
  // Desligar todas as saídas
  digitalWrite(PIN_FAROL, LOW);
  digitalWrite(PIN_DRL, LOW);
  digitalWrite(PIN_INTERNA, LOW);
  digitalWrite(PIN_PES, LOW);
  
  // Inicializar sistema de arquivos
  initFileSystem();
  loadConfig();
  
  // Configurar WiFi em modo AP (inicia ligado na primeira vez)
  if (wifiEnabled) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifiSSID.c_str(), AP_PASSWORD);
    IPAddress IP = WiFi.softAPIP();
    pushLog(String("AP iniciado: ") + wifiSSID);
    pushLog(String("Endereço IP: ") + IP.toString());
    pushLog("💡 Dica: Ligue/desligue ignição 5x em 10s para desligar WiFi");
  } else {
    WiFi.mode(WIFI_OFF);
    pushLog("WiFi desligado (economia de bateria)");
  }
  
  // ===== ROTAS DO SERVIDOR =====
  
  // Servir página HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "Arquivo index.html não encontrado");
    }
  });
  
  // Obter configuração atual
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    String response;
    serializeJson(configDoc, response);
    request->send(200, "application/json", response);
  });
  
  // Salvar nova configuração
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      if (!request->authenticate(AUTH_USER, AUTH_PASS)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"unauthorized\"}");
        return;
      }
      static String jsonBuffer;
      
      if (index == 0) jsonBuffer = "";
      jsonBuffer += String((char*)data).substring(0, len);
      
      if (index + len == total) {
        DeserializationError error = deserializeJson(configDoc, jsonBuffer);
        
        if (error) {
          request->send(400, "application/json", "{\"success\":false,\"error\":\"JSON inválido\"}");
          return;
        }
        
        saveConfig(jsonBuffer);
        request->send(200, "application/json", "{\"success\":true}");
      }
    }
  );
  
  // Obter/Alterar configurações de WiFi e BCM Name
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    JsonDocument doc;
    doc["wifiSSID"] = wifiSSID;
    doc["bcmName"] = bcmName;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  
  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      if (!request->authenticate(AUTH_USER, AUTH_PASS)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"unauthorized\"}");
        return;
      }
      static String jsonBuffer;
      
      if (index == 0) jsonBuffer = "";
      jsonBuffer += String((char*)data).substring(0, len);
      
      if (index + len == total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonBuffer);
        
        if (error) {
          request->send(400, "application/json", "{\"success\":false,\"error\":\"JSON inválido\"}");
          return;
        }
        
        // Atualizar SSID
        if (doc["wifiSSID"].is<const char*>() && strlen(doc["wifiSSID"]) > 0) {
          wifiSSID = doc["wifiSSID"].as<const char*>();
          configDoc["wifiSSID"] = wifiSSID;
          pushLogf("WiFi SSID alterado para: %s", wifiSSID.c_str());
        }
        
        // Atualizar BCM Name
        if (doc["bcmName"].is<const char*>() && strlen(doc["bcmName"]) > 0) {
          bcmName = doc["bcmName"].as<const char*>();
          configDoc["bcmName"] = bcmName;
          pushLogf("BCM Name alterado para: %s", bcmName.c_str());
        }
        
        // Salvar configuração
        String jsonStr;
        serializeJson(configDoc, jsonStr);
        saveConfig(jsonStr);
        
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Configurações atualizadas. WiFi será reiniciado na próxima vez que for ativado.\"}");
      }
    }
  );
  
  // Resetar configuração
  server.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    configDoc.clear();
    LittleFS.remove("/config.json");
    request->send(200, "application/json", "{\"success\":true}");
  });
  
  // Reiniciar ESP32
  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    request->send(200, "application/json", "{\"success\":true,\"message\":\"ESP32 reiniciando...\"}");
    delay(500);
    ESP.restart();
  });
  
  // Status do sistema
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    
    JsonObject inputs = doc["inputs"].to<JsonObject>();
    inputs["ignicao"] = digitalRead(PIN_IGNICAO) == LOW;
    inputs["porta"] = digitalRead(PIN_PORTA) == LOW;
    inputs["trava"] = digitalRead(PIN_TRAVA) == LOW;
    inputs["destrava"] = digitalRead(PIN_DESTRAVA) == LOW;
    
    JsonObject outputs = doc["outputs"].to<JsonObject>();
    outputs["farol"] = digitalRead(PIN_FAROL);
    outputs["drl"] = digitalRead(PIN_DRL);
    outputs["interna"] = digitalRead(PIN_INTERNA);
    outputs["pes"] = digitalRead(PIN_PES);
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  
  // Controle manual de saídas (toggle)
  server.on("/api/output/toggle", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      if (!request->authenticate(AUTH_USER, AUTH_PASS)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"unauthorized\"}");
        return;
      }
      static String jsonBuffer;
      
      if (index == 0) jsonBuffer = "";
      jsonBuffer += String((char*)data).substring(0, len);
      
      if (index + len == total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonBuffer);
        
        if (error) {
          request->send(400, "application/json", "{\"success\":false,\"error\":\"JSON inválido\"}");
          return;
        }
        
        const char* output = doc["output"];
        if (!output) {
          request->send(400, "application/json", "{\"success\":false,\"error\":\"Campo 'output' obrigatório\"}");
          return;
        }
        
        // Encontrar a saída
        int outputIndex = -1;
        for (int i = 0; i < 4; i++) {
          if (strcmp(outputNames[i], output) == 0) {
            outputIndex = i;
            break;
          }
        }
        
        if (outputIndex == -1) {
          request->send(400, "application/json", "{\"success\":false,\"error\":\"Saída inválida\"}");
          return;
        }
        
        // Toggle do estado da saída
        int currentState = digitalRead(outputPins[outputIndex]);
        int newState = !currentState;
        digitalWrite(outputPins[outputIndex], newState);
        
        pushLogf("🔧 Controle manual: %s -> %s", outputNames[outputIndex], newState ? "ON" : "OFF");
        
        request->send(200, "application/json", "{\"success\":true}");
      }
    }
  );

  // Logs recentes
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    JsonDocument doc;
    JsonArray arr = doc["logs"].to<JsonArray>();
    // retornar do mais antigo ao mais recente
    for (size_t i = 0; i < logCount; i++) {
      size_t idx = (logHead + LOG_CAPACITY - logCount + i) % LOG_CAPACITY;
      arr.add(logBuffer[idx]);
    }
    String payload;
    serializeJson(doc, payload);
    request->send(200, "application/json", payload);
  });
  
  server.begin();
  pushLog("Servidor HTTP iniciado");
  pushLog("Acesse: http://192.168.4.1");
}

// ============= LOOP =============

void loop() {
  checkInputs();
  updateOutputs();
  
  // Verificar fail-safe por timeout de ignição desligada
  if (ignicaoOffTimeoutActive && !failsafeTriggered) {
    if (millis() - ignicaoOffTime >= TIMEOUT_IGNICAO_OFF) {
      shutdownAllOutputs();
      ignicaoOffTimeoutActive = false;
    }
  }
  
  delay(10); // Pequeno delay para estabilidade
}
