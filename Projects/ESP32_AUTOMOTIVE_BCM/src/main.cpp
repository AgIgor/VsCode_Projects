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
struct EntityState {
  bool current = false;
  bool previous = false;
  unsigned long lastChange = 0;
  const uint32_t debounce = 50;
  unsigned long ligarTime = 0;
  unsigned long desligarTime = 0;
};

// ============= VARIÁVEIS GLOBAIS =============
AsyncWebServer server(80);
JsonDocument configDoc;

// WiFi e BCM configuráveis
String wifiSSID = DEFAULT_SSID;
String bcmName = DEFAULT_BCM_NAME;

// Estados de todas as entidades (4 inputs + 4 outputs)
EntityState entityStates[8]; 

// Mapeamento de entidades
const char* entityNames[] = {"ignicao", "porta", "trava", "destrava", "farol", "drl", "luz-interna", "luz-assoalho"};
const uint8_t inputPins[] = {PIN_IGNICAO, PIN_PORTA, PIN_TRAVA, PIN_DESTRAVA};
const uint8_t outputPins[] = {PIN_FAROL, PIN_DRL, PIN_INTERNA, PIN_PES};

// Fail-safe: controle de timeout
unsigned long ignicaoOffTime = 0;
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
    entityStates[i + 4].current = false;
    entityStates[i + 4].ligarTime = 0;
    entityStates[i + 4].desligarTime = 0;
    pushLogf("  ✗ %s desligado", entityNames[i + 4]);
  }
  disableWiFi();
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

  // Carregar WiFi SSID e BCM Name
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
  pushLog("Configuração carregada OK");
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

// Encontrar índice da entidade pelo nome
int getEntityIndex(const char* name) {
  for (int i = 0; i < 8; i++) {
    if (strcmp(entityNames[i], name) == 0) return i;
  }
  return -1;
}

// Verificar se condição é atendida
bool checkStateCondition(const char* entityName, const char* stateName) {
  int idx = getEntityIndex(entityName);
  if (idx == -1) return true; // Entidade não encontrada, ignora
  
  // Mapeamento de estados
  bool expectedState = false;
  if (strcmp(stateName, "ligado") == 0 || strcmp(stateName, "aberto") == 0) {
    expectedState = true;
  } else if (strcmp(stateName, "desligado") == 0 || strcmp(stateName, "fechado") == 0) {
    expectedState = false;
  }
  
  return entityStates[idx].current == expectedState;
}

// Processar regras quando uma entidade muda de estado
void processStateChange(int entityIndex, const char* eventType) {
  const char* entityName = entityNames[entityIndex];
  pushLogf("⚡ Mudança: %s %s", entityName, eventType);
  
  if (!configDoc["logicas"].is<JsonArray>()) return;
  
  JsonArray logicas = configDoc["logicas"].as<JsonArray>();
  
  for (JsonObject logica : logicas) {
    // Verificar se o trigger bate
    JsonObject trigger = logica["trigger"];
    const char* triggerEntity = trigger["entity"];
    const char* triggerEvent = trigger["event"];
    
    if (strcmp(triggerEntity, entityName) != 0) continue;
    if (strcmp(triggerEvent, eventType) != 0) continue;
    
    pushLogf("  📋 Regra encontrada: %s %s", triggerEntity, triggerEvent);
    
    // Verificar condições com suporte a AND (e) e OR (ou)
    bool conditionsMet = true;
    if (logica["conditions"].is<JsonArray>()) {
      JsonArray conditions = logica["conditions"].as<JsonArray>();
      bool andResult = true;  // Acumula resultados de AND
      bool orResult = false;  // Acumula resultados de OR
      bool inOrGroup = false; // Se está processando um grupo OR
      
      for (int condIdx = 0; condIdx < conditions.size(); condIdx++) {
        JsonObject cond = conditions[condIdx];
        const char* condEntity = cond["entity"];
        const char* condState = cond["state"];
        const char* opStr = cond["operator"] | "";
        
        bool condResult = checkStateCondition(condEntity, condState);
        pushLogf("     Condicao: %s %s = %s", condEntity, condState, condResult ? "OK" : "FALHA");
        
        // Determinar operador (null/vazio = 'e')
        bool isOrOperator = (strcmp(opStr, "ou") == 0);
        
        if (condIdx == 0) {
          // Primeira condição
          andResult = condResult;
          inOrGroup = false;
        } else if (isOrOperator) {
          // Operador é OR: se não estávamos em OR, começa grupo OR
          if (!inOrGroup) {
            orResult = andResult || condResult;
            inOrGroup = true;
          } else {
            // Já estava em OR, continua OR
            orResult = orResult || condResult;
          }
        } else {
          // Operador é AND (ou null)
          if (inOrGroup) {
            // Sair do grupo OR e voltar para AND
            andResult = orResult && condResult;
            inOrGroup = false;
          } else {
            // Continuar AND
            andResult = andResult && condResult;
          }
        }
      }
      
      // Resultado final
      if (inOrGroup) {
        conditionsMet = orResult;
      } else {
        conditionsMet = andResult;
      }
    }
    
    if (!conditionsMet) continue;
    
    pushLog("  ✓ Condições atendidas, executando ações:");
    
    // Executar ações
    if (logica["actions"].is<JsonArray>()) {
      JsonArray actions = logica["actions"].as<JsonArray>();
      unsigned long now = millis();
      
      for (JsonObject action : actions) {
        const char* outputName = action["output"];
        int outputIdx = getEntityIndex(outputName);
        
        if (outputIdx < 4 || outputIdx >= 8) continue; // Deve ser saída (4-7)
        
        // Processar "ligar": se existe e não é null, usa o valor; senão -1
        int ligarAfter = -1;
        if (action.containsKey("ligar") && !action["ligar"].isNull()) {
          ligarAfter = action["ligar"].as<int>();
        }
        
        // Processar "desligar": se existe e não é null, usa o valor; senão -1
        int desligarAfter = -1;
        if (action.containsKey("desligar") && !action["desligar"].isNull()) {
          desligarAfter = action["desligar"].as<int>();
        }
        
        // Agendar ações baseado nos valores
        if (ligarAfter >= 0) {
          // Agendar para ligar após X segundos
          entityStates[outputIdx].ligarTime = now + (ligarAfter * 1000);
          pushLogf("     💡 %s: ligar em %ds", outputName, ligarAfter);
          
          // Se desligar também foi especificado, é a DURAÇÃO ligado
          if (desligarAfter >= 0) {
            entityStates[outputIdx].desligarTime = now + (ligarAfter * 1000) + (desligarAfter * 1000);
            pushLogf("     💡 %s: desligar após %ds ligado (total %ds)", outputName, desligarAfter, ligarAfter + desligarAfter);
          } else {
            // Sem desligar = fica ligado indefinidamente
            entityStates[outputIdx].desligarTime = 0;
          }
        } else if (desligarAfter >= 0) {
          // Apenas desligar (sem ligar antes)
          entityStates[outputIdx].desligarTime = now + (desligarAfter * 1000);
          pushLogf("     💡 %s: desligar em %ds", outputName, desligarAfter);
        }
      }
    }
  }
}

// Atualizar estado das saídas com base em temporizadores
void updateOutputs() {
  unsigned long now = millis();
  
  for (int i = 4; i < 8; i++) { // Outputs são índices 4-7
    int outputPin = outputPins[i - 4];
    
    // Verifica se deve ligar
    if (entityStates[i].ligarTime > 0 && now >= entityStates[i].ligarTime) {
      digitalWrite(outputPin, HIGH);
      bool wasOn = entityStates[i].current;
      entityStates[i].current = true;
      entityStates[i].ligarTime = 0;
      pushLogf("✓ %s LIGADO", entityNames[i]);
      
      // Se estava desligado e agora ligou, disparar evento "ligou"
      if (!wasOn) {
        entityStates[i].previous = false;
        processStateChange(i, "ligou");
      }
    }
    
    // Verifica se deve desligar
    if (entityStates[i].desligarTime > 0 && now >= entityStates[i].desligarTime) {
      digitalWrite(outputPin, LOW);
      bool wasOn = entityStates[i].current;
      entityStates[i].current = false;
      entityStates[i].desligarTime = 0;
      pushLogf("✗ %s DESLIGADO", entityNames[i]);
      
      // Se estava ligado e agora desligou, disparar evento "desligou"
      if (wasOn) {
        entityStates[i].previous = true;
        processStateChange(i, "desligou");
      }
    }
  }
}

// Monitorar entradas e disparar eventos de mudança
void checkInputs() {
  unsigned long now = millis();
  
  for (int i = 0; i < 4; i++) {
    bool reading = (digitalRead(inputPins[i]) == LOW); // LOW = ativado (pull-up)
    
    // Debounce
    if (reading != entityStates[i].current) {
      if (now - entityStates[i].lastChange > entityStates[i].debounce) {
        entityStates[i].previous = entityStates[i].current;
        entityStates[i].current = reading;
        entityStates[i].lastChange = now;
        
        // Detectar tipo de mudança
        const char* eventType = nullptr;
        
        if (i == 0) { // ignição
          if (reading) { // Ligou
            eventType = "ligou";
            ignicaoOffTimeoutActive = false;
            failsafeTriggered = false;
            
            // Contador para toggle WiFi
            if (now - lastIgnicaoToggle > IGNICAO_TOGGLE_WINDOW) {
              ignicaoToggleCount = 0;
            }
            ignicaoToggleCount++;
            lastIgnicaoToggle = now;
            
            if (ignicaoToggleCount >= 5) {
              if (wifiEnabled) disableWiFi();
              else enableWiFi();
              ignicaoToggleCount = 0;
            } else {
              pushLogf("🔑 Ignição %d/5 (toggle WiFi)", ignicaoToggleCount);
            }
          } else { // Desligou
            eventType = "desligou";
            ignicaoOffTime = millis();
            ignicaoOffTimeoutActive = true;
            failsafeTriggered = false;
            pushLogf("⏱️ Fail-safe ativo em %ds", TIMEOUT_IGNICAO_OFF/1000);
          }
        } else if (i == 1) { // porta
          eventType = reading ? "abriu" : "fechou";
        } else if (i == 2) { // trava
          if (reading) eventType = "ligou"; // Apenas quando pressionado
        } else if (i == 3) { // destrava
          if (reading) eventType = "ligou"; // Apenas quando pressionado
        }
        
        if (eventType) {
          processStateChange(i, eventType);
        }
      }
    }
  }
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
  for (int i = 0; i < 4; i++) {
    digitalWrite(outputPins[i], LOW);
  }
  
  // Inicializar sistema de arquivos
  initFileSystem();
  loadConfig();
  
  // Configurar WiFi em modo AP
  if (wifiEnabled) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifiSSID.c_str(), AP_PASSWORD);
    IPAddress IP = WiFi.softAPIP();
    pushLog(String("AP: ") + wifiSSID);
    pushLog(String("IP: ") + IP.toString());
    pushLog("💡 5x ignição em 10s = toggle WiFi");
  } else {
    WiFi.mode(WIFI_OFF);
  }
  
  // ===== ROTAS DO SERVIDOR =====
  
  // Servir página HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "index.html não encontrado");
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
        request->send(401, "application/json", "{\"success\":false}");
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
        request->send(401, "application/json", "{\"success\":false}");
        return;
      }
      static String jsonBuffer;
      
      if (index == 0) jsonBuffer = "";
      jsonBuffer += String((char*)data).substring(0, len);
      
      if (index + len == total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonBuffer);
        
        if (error) {
          request->send(400, "application/json", "{\"success\":false}");
          return;
        }
        
        if (doc["wifiSSID"].is<const char*>() && strlen(doc["wifiSSID"]) > 0) {
          wifiSSID = doc["wifiSSID"].as<const char*>();
          configDoc["wifiSSID"] = wifiSSID;
        }
        
        if (doc["bcmName"].is<const char*>() && strlen(doc["bcmName"]) > 0) {
          bcmName = doc["bcmName"].as<const char*>();
          configDoc["bcmName"] = bcmName;
        }
        
        String jsonStr;
        serializeJson(configDoc, jsonStr);
        saveConfig(jsonStr);
        
        request->send(200, "application/json", "{\"success\":true}");
      }
    }
  );
  
  // Reiniciar ESP32
  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    request->send(200, "application/json", "{\"success\":true}");
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
    inputs["ignicao"] = entityStates[0].current;
    inputs["porta"] = entityStates[1].current;
    inputs["trava"] = entityStates[2].current;
    inputs["destrava"] = entityStates[3].current;
    
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
        request->send(401, "application/json", "{\"success\":false}");
        return;
      }
      static String jsonBuffer;
      
      if (index == 0) jsonBuffer = "";
      jsonBuffer += String((char*)data).substring(0, len);
      
      if (index + len == total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonBuffer);
        
        if (error) {
          request->send(400, "application/json", "{\"success\":false}");
          return;
        }
        
        const char* output = doc["output"];
        if (!output) {
          request->send(400, "application/json", "{\"success\":false}");
          return;
        }
        
        int outputIndex = getEntityIndex(output);
        if (outputIndex < 4 || outputIndex >= 8) {
          request->send(400, "application/json", "{\"success\":false}");
          return;
        }
        
        int pinIdx = outputIndex - 4;
        int currentState = digitalRead(outputPins[pinIdx]);
        int newState = !currentState;
        digitalWrite(outputPins[pinIdx], newState);
        entityStates[outputIndex].current = newState;
        
        pushLogf("🔧 Manual: %s -> %s", output, newState ? "ON" : "OFF");
        
        request->send(200, "application/json", "{\"success\":true}");
      }
    }
  );

  // Logs recentes
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    JsonDocument doc;
    JsonArray arr = doc["logs"].to<JsonArray>();
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
  pushLog("http://192.168.4.1");
}

// ============= LOOP =============

void loop() {
  checkInputs();
  updateOutputs();
  
  // Verificar fail-safe
  if (ignicaoOffTimeoutActive && !failsafeTriggered) {
    if (millis() - ignicaoOffTime >= TIMEOUT_IGNICAO_OFF) {
      shutdownAllOutputs();
      ignicaoOffTimeoutActive = false;
    }
  }
  
  delay(10);
}
