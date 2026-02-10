#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ============= PINOS =============
#define PIN_IGNICAO 13
#define PIN_PORTA 14
#define PIN_TRAVA 25
#define PIN_DESTRAVA 26

#define PIN_FAROL 16
#define PIN_DRL 17
#define PIN_LUZ_TETO 18      // luz-interna
#define PIN_LUZ_PE 19        // luz-assoalho

// ============= TIMERS =============
#define TIMER_1MIN 60000     // 1 minuto em ms
#define DEBOUNCE 50

// ============= CONFIGURAÇÃO =============
#define AP_SSID "BCM_ESP32"
#define AP_PASSWORD "12345678"
#define AUTH_USER "admin"
#define AUTH_PASS "admin123"

// ============= SERVIDOR =============
AsyncWebServer server(80);
JsonDocument configDoc;

// ============= ESTRUTURA DE AÇÃO =============
struct Action {
  const char* rule;          // nome da regra
  bool state;               // ON/OFF desejado
  unsigned long timestamp;  // quando foi acionada
};

// ============= VARIÁVEIS GLOBAIS =============

// Inputs (sensores)
struct {
  bool ignicao = false;
  bool ignicao_prev = false;
  bool porta = false;
  bool porta_prev = false;
  bool trava = false;
  bool trava_prev = false;
  bool destrava = false;
  bool destrava_prev = false;
  unsigned long lastDebounce = 0;
} inputs;

// Outputs (ações em progresso)
struct {
  Action farol[5];         // até 5 ações simultâneas por saída
  Action drl[5];
  Action luz_teto[5];
  Action luz_pe[5];
  uint8_t count_farol = 0;
  uint8_t count_drl = 0;
  uint8_t count_luz_teto = 0;
  uint8_t count_luz_pe = 0;
} actions;

// Timers
unsigned long tracer_timer = 0;
unsigned long destrava_timer = 0;
bool traver_ativo = false;
bool destrava_ativo = false;

// Logs
String logBuffer[50];
uint8_t logIndex = 0;

void pushLog(String msg) {
  logBuffer[logIndex] = msg;
  logIndex = (logIndex + 1) % 50;
  Serial.println(msg);
}

// ============= FUNÇÕES DE AÇÃO =============

void addAction(Action* list, uint8_t& count, const char* rule, bool state, const char* output = "") {
  // Se trava está ativo, ignora ações de ignição em DRL e luz_teto
  if (traver_ativo && strcmp(rule, "ignicao") == 0) {
    if (strcmp(output, "drl") == 0 || strcmp(output, "luz_teto") == 0) {
      pushLog(String("⚠️ BLOQUEADO: Ignição não pode controlar ") + output + " durante trava");
      return;
    }
  }

  if (count < 5) {
    list[count].rule = rule;
    list[count].state = state;
    list[count].timestamp = millis();
    count++;
    pushLog(String("➕ ") + rule + ": " + output + " = " + (state ? "ON" : "OFF"));
  }
}

void clearAction(Action* list, uint8_t& count, const char* rule) {
  for (int i = 0; i < count; i++) {
    if (strcmp(list[i].rule, rule) == 0) {
      // Remove pela troca com o último
      list[i] = list[count - 1];
      count--;
      i--;
    }
  }
  pushLog(String("➖ Remove: ") + rule);
}

// Retorna o estado final (última ação vence)
bool getOutputState(Action* list, uint8_t count) {
  if (count == 0) return false;
  return list[count - 1].state;  // A última ação é a mais recente
}

void updateOutputs() {
  bool farol_state = getOutputState(actions.farol, actions.count_farol);
  bool drl_state = getOutputState(actions.drl, actions.count_drl);
  bool teto_state = getOutputState(actions.luz_teto, actions.count_luz_teto);
  bool pe_state = getOutputState(actions.luz_pe, actions.count_luz_pe);

  digitalWrite(PIN_FAROL, farol_state ? HIGH : LOW);
  digitalWrite(PIN_DRL, drl_state ? HIGH : LOW);
  digitalWrite(PIN_LUZ_TETO, teto_state ? HIGH : LOW);
  digitalWrite(PIN_LUZ_PE, pe_state ? HIGH : LOW);

  pushLog(String("💡 Estados: F=") + farol_state + " DRL=" + drl_state + 
          " Teto=" + teto_state + " Pé=" + pe_state);
}

// ============= REGRAS DE COMPORTAMENTO =============

void onIgnicaoLigou() {
  pushLog("🔥 IGNIÇÃO LIGOU");
  addAction(actions.drl, actions.count_drl, "ignicao", true, "drl");
  addAction(actions.luz_pe, actions.count_luz_pe, "ignicao", true, "luz_pe");
}

void onIgnicaoDesligou() {
  pushLog("❄️ IGNIÇÃO DESLIGOU");
  clearAction(actions.drl, actions.count_drl, "ignicao");
  clearAction(actions.luz_pe, actions.count_luz_pe, "ignicao");
  addAction(actions.luz_teto, actions.count_luz_teto, "ignicao", true, "luz_teto");
  addAction(actions.drl, actions.count_drl, "ignicao", true, "drl");
}

void onPortaAbriu() {
  pushLog("🚪 PORTA ABRIU");
  // Se luz teto está OFF, liga
  bool teto_current = getOutputState(actions.luz_teto, actions.count_luz_teto);
  if (!teto_current) {
    addAction(actions.luz_teto, actions.count_luz_teto, "porta", true, "luz_teto");
  }
}

void onPortaFechou() {
  pushLog("🚪 PORTA FECHOU");
  // Desliga luz de teto (mantém luz pé ligada pela ignição)
  addAction(actions.luz_teto, actions.count_luz_teto, "porta", false, "luz_teto");
}

void onTravaLigou() {
  pushLog("🔒 TRAVA LIGOU - Timer 60s iniciado");
  traver_ativo = true;
  tracer_timer = millis();
  
  // Desliga luz teto e DRL
  addAction(actions.luz_teto, actions.count_luz_teto, "trava", false, "luz_teto");
  addAction(actions.drl, actions.count_drl, "trava", false, "drl");
  
  // Mantém farol por 30s, depois desliga tudo
  addAction(actions.farol, actions.count_farol, "trava", true, "farol");
}

void onDestraVaLigou() {
  pushLog("🔓 DESTRAVA LIGOU - Timer 60s iniciado");
  destrava_ativo = true;
  destrava_timer = millis();
  
  // Liga farol e luz teto
  addAction(actions.farol, actions.count_farol, "destrava", true, "farol");
  addAction(actions.luz_teto, actions.count_luz_teto, "destrava", true, "luz_teto");
}

// ============= TIMERS =============

void checkTravaTimer() {
  if (!traver_ativo) return;
  
  if (millis() - tracer_timer > TIMER_1MIN) {
    pushLog("⏱️ Timer Trava expirou - desligando tudo");
    clearAction(actions.farol, actions.count_farol, "trava");
    clearAction(actions.luz_teto, actions.count_luz_teto, "trava");
    clearAction(actions.drl, actions.count_drl, "trava");
    traver_ativo = false;
  }
}

void checkDestravaTimer() {
  if (!destrava_ativo) return;
  
  if (millis() - destrava_timer > TIMER_1MIN) {
    pushLog("⏱️ Timer Destrava expirado - removendo ações");
    clearAction(actions.farol, actions.count_farol, "destrava");
    clearAction(actions.luz_teto, actions.count_luz_teto, "destrava");
    destrava_ativo = false;
  }
}

// ============= LEITURA DE INPUTS =============

void readInputs() {
  unsigned long now = millis();
  if (now - inputs.lastDebounce < DEBOUNCE) return;
  inputs.lastDebounce = now;

  // Ler pins (LOW = ativado por pull-up)
  bool ig = (digitalRead(PIN_IGNICAO) == LOW);
  bool pt = (digitalRead(PIN_PORTA) == LOW);
  bool tr = (digitalRead(PIN_TRAVA) == LOW);
  bool ds = (digitalRead(PIN_DESTRAVA) == LOW);

  // Detectar mudanças
  if (ig && !inputs.ignicao) onIgnicaoLigou();
  if (!ig && inputs.ignicao) onIgnicaoDesligou();
  
  if (pt && !inputs.porta) onPortaAbriu();
  if (!pt && inputs.porta) onPortaFechou();
  
  if (tr && !inputs.trava) onTravaLigou();
  if (ds && !inputs.destrava) onDestraVaLigou();

  // Atualizar estado anterior
  inputs.ignicao = ig;
  inputs.porta = pt;
  inputs.trava = tr;
  inputs.destrava = ds;
}

// ============= SETUP =============

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Pinos de entrada (pull-up ativo)
  pinMode(PIN_IGNICAO, INPUT_PULLUP);
  pinMode(PIN_PORTA, INPUT_PULLUP);
  pinMode(PIN_TRAVA, INPUT_PULLUP);
  pinMode(PIN_DESTRAVA, INPUT_PULLUP);
  
  // Pinos de saída
  pinMode(PIN_FAROL, OUTPUT);
  pinMode(PIN_DRL, OUTPUT);
  pinMode(PIN_LUZ_TETO, OUTPUT);
  pinMode(PIN_LUZ_PE, OUTPUT);
  
  // Iniciar tudo desligado
  digitalWrite(PIN_FAROL, LOW);
  digitalWrite(PIN_DRL, LOW);
  digitalWrite(PIN_LUZ_TETO, LOW);
  digitalWrite(PIN_LUZ_PE, LOW);
  
  // LittleFS
  LittleFS.begin();
  
  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  pushLog("✅ BCM Iniciado");
  
  // API REST
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->authenticate(AUTH_USER, AUTH_PASS);
    JsonDocument doc;
    doc["ignicao"] = inputs.ignicao;
    doc["porta"] = inputs.porta;
    doc["trava"] = inputs.trava;
    doc["destrava"] = inputs.destrava;
    doc["farol"] = getOutputState(actions.farol, actions.count_farol);
    doc["drl"] = getOutputState(actions.drl, actions.count_drl);
    doc["luz_teto"] = getOutputState(actions.luz_teto, actions.count_luz_teto);
    doc["luz_pe"] = getOutputState(actions.luz_pe, actions.count_luz_pe);
    String response;
    serializeJson(doc, response);
    req->send(200, "application/json", response);
  });
  
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->authenticate(AUTH_USER, AUTH_PASS);
    JsonDocument doc;
    
    // Última ação (último log)
    String lastLog = "";
    for (int i = 49; i >= 0; i--) {
      if (logBuffer[i].length() > 0) {
        lastLog = logBuffer[i];
        break;
      }
    }
    doc["ultimaacao"] = lastLog;
    
    // Timers ativos
    unsigned long now = millis();
    JsonObject timers = doc.createNestedObject("timers");
    
    if (traver_ativo) {
      unsigned long elapsed = now - tracer_timer;
      unsigned long remaining = (elapsed > TIMER_1MIN) ? 0 : TIMER_1MIN - elapsed;
      timers["trava_ms"] = remaining / 1000;
    }
    
    if (destrava_ativo) {
      unsigned long elapsed = now - destrava_timer;
      unsigned long remaining = (elapsed > TIMER_1MIN) ? 0 : TIMER_1MIN - elapsed;
      timers["destrava_ms"] = remaining / 1000;
    }
    
    String response;
    serializeJson(doc, response);
    req->send(200, "application/json", response);
  });
  
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.begin();
}

// ============= LOOP =============

void loop() {
  readInputs();
  checkTravaTimer();
  checkDestravaTimer();
  updateOutputs();
  delay(100);
}
