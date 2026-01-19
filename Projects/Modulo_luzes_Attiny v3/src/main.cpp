#include <Arduino.h>
#ifdef __AVR__
  #include <avr/wdt.h>
  #include <avr/interrupt.h>
#endif

#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
  #include <EEPROM.h>
  #include "../include/webpage.h"
  
  // Configurações de WiFi AP
  const char* SSID_AP = "Modulo_Luzes";
  const char* PASSWORD_AP = "12345678";
  WebServer server(80);
  
  #define EEPROM_SIZE 512
  #define EEPROM_ADDR_CONFIG 0
#endif

/* ===== CONFIGURAÇÃO DE PINOS ===== */
#ifdef __AVR_ATtiny85__
  #define PIN_SETA        0
  #define PIN_IGNICAO     2
  #define PIN_FAROL       1
  #define PIN_RELE_SETA   4
  #define PIN_TRAVA       -1  // Não disponível em ATtiny
  #define USE_WATCHDOG    1
  #define SUPORTA_TRAVA   0
#elif __AVR_ATmega328P__
  #define PIN_SETA        4  
  #define PIN_IGNICAO     7  
  #define PIN_FAROL       12 
  #define PIN_RELE_SETA   13 
  #define PIN_TRAVA       -1  // Não disponível em Arduino Uno
  #define USE_WATCHDOG    1
  #define SUPORTA_TRAVA   0
#elif defined(ESP32)
  #define PIN_SETA        13
  #define PIN_IGNICAO     12
  #define PIN_FAROL       14
  #define PIN_RELE_SETA   27
  #define PIN_TRAVA       26  // Pino para detectar trava/destrava
  #define USE_WATCHDOG    0
  #define SUPORTA_TRAVA   1
#endif

/* ===== TIMINGS (ms) - AGORA VARIÁVEIS ===== */
unsigned long JANELA_PISCADA = 1200;
unsigned long TEMPO_FOLLOW_1 = 40000;
unsigned long TEMPO_FOLLOW_2 = 25000;
unsigned long TEMPO_POS_IGNICAO = 10000;
unsigned long TEMPO_MIN_RELE_SETA = 2500;
unsigned long TEMPO_SETA_INATIVA = 3000;
unsigned long TEMPO_DEBOUNCE = 20;
unsigned long TEMPO_LEITURA = 150;
unsigned long TEMPO_LUZ_TRAVA = 3000;   // Tempo que farol fica ligado ao trancar/destrancar

/* ===== OPÇÕES DE FUNCIONALIDADES ===== */
bool ativarLuzTrava = true;            // Ativa/desativa luzes ao trancar/destrancar
bool ativarLuzPosPosChave = true;      // Ativa/desativa luzes ao ligar a chave
bool ativarFollowMe = true;            // Ativa/desativa Follow-Me (piscadas de seta)
bool ativarDrlPosChave = true;         // Ativa/desativa DRL após desligar a chave

/* ===== ESTADO DO SISTEMA ===== */
unsigned long agora = 0;

// Variáveis de debounce para entradas
unsigned long ultimaLeituraIgnicao = 0;
unsigned long ultimaLeituraSeta = 0;
#if SUPORTA_TRAVA
  unsigned long ultimaLeituraTrava = 0;
#endif
bool ignicaoDebounced = false;
bool setaDebounced = false;
#if SUPORTA_TRAVA
  bool travaDebounced = false;
#endif
bool ignicaoAnterior = false;
bool setaAnterior = false;
#if SUPORTA_TRAVA
  bool travaAnterior = false;
#endif

// Controle de farol
bool farolLigado = false;

// Estados do modo DRL (Daytime Running Light - ignição ligada)
bool modoAtivo = false;  // Ignição ligada

// Estados do Follow-Me (ignição desligada)
unsigned long timerFollowMe = 0;
unsigned long tempoFollowMe = 0;
bool followMeAtivo = false;
unsigned long ultimaPiscada = 0;
int contPiscadas = 0;

// Estados pós-ignição (farol mantém aceso após desligar)
unsigned long timerPosIgnicao = 0;
bool posIgnicaoAtivo = false;

// Controle do relé das setas
unsigned long timerReleSeta = 0;
bool releSetaAtivo = false;
unsigned long ultimaAtividadeSeta = 0;

// Estados da trava/destrava (ESP32)
#if SUPORTA_TRAVA
  unsigned long timerLuzTrava = 0;
  bool luzTravaAtiva = false;
#endif

/* ===== FUNÇÕES AUXILIARES ===== */

/**
 * @brief Debounce digital de uma entrada com acumulador temporal
 * Lê a entrada apenas a cada TEMPO_DEBOUNCE milissegundos
 * @param pino: Pino a ler (com INPUT_PULLUP)
 * @param ultimoTempo: Referência ao último tempo de leitura
 * @param estadoAnterior: Estado anterior da entrada
 * @return Estado debounced (invertido para entrada PULLUP)
 */
bool lerEntradaDebounced(uint8_t pino, unsigned long& ultimoTempo, bool estadoAnterior) {
  if (agora - ultimoTempo >= TEMPO_DEBOUNCE) {
    bool estadoAtual = !digitalRead(pino);  // Invertido porque usa PULLUP
    ultimoTempo = agora;
    return estadoAtual;
  }
  return estadoAnterior;
}

/**
 * @brief Liga ou desliga o farol de forma centralizada
 * Evita múltiplas alterações desnecessárias ao pino
 * @param ligar: true para ligar, false para desligar
 */
void controlarFarol(bool ligar) {
  if (ligar && !farolLigado) {
    digitalWrite(PIN_FAROL, HIGH);
    farolLigado = true;
  } else if (!ligar && farolLigado) {
    digitalWrite(PIN_FAROL, LOW);
    farolLigado = false;
  }
}

/**
 * @brief Processa o Follow-Me (acende farol por piscadas de seta com ignição desligada)
 * 
 * Funcionamento:
 * - Com ignição desligada, conta piscadas de seta dentro de JANELA_PISCADA
 * - 1 piscada = farol por TEMPO_FOLLOW_1 (40s)
 * - 2+ piscadas = farol por TEMPO_FOLLOW_2 (25s)
 */
void procesarFollowMe() {
  if (modoAtivo || !ativarFollowMe) return;  // Follow-Me desativado ou ignição ligada
  
  // Detecta piscada (borda de subida na seta)
  if (setaDebounced && !setaAnterior) {
    contPiscadas++;
    ultimaPiscada = agora;
  }

  // Quando a janela de piscadas fecha, ativa o Follow-Me
  if (contPiscadas > 0 && (agora - ultimaPiscada > JANELA_PISCADA)) {
    
    if (contPiscadas == 1) {
      tempoFollowMe = TEMPO_FOLLOW_1;
    } else {
      tempoFollowMe = TEMPO_FOLLOW_2;
    }

    if (tempoFollowMe > 0) {
      controlarFarol(true);
      followMeAtivo = true;
      timerFollowMe = agora;
    }
    
    contPiscadas = 0;
  }

  // Desativa Follow-Me após expiração do tempo
  if (followMeAtivo && (agora - timerFollowMe >= tempoFollowMe)) {
    followMeAtivo = false;
    tempoFollowMe = 0;
    
    // Apaga farol se não houver outro modo ativo
    if (!posIgnicaoAtivo) {
      controlarFarol(false);
    }
  }
}

/**
 * @brief Controla o farol em modo DRL (Daytime Running Light) quando ignição ligada
 */
void processarModoAtivo() {
  if (modoAtivo && ativarLuzPosPosChave) {  // Verifica se DRL está ativado
    controlarFarol(true);
  } else if (modoAtivo && !ativarLuzPosPosChave) {
    // Se DRL desativado, não liga o farol automaticamente
    // mas não força desligar caso outro modo esteja ativo
  }
}

/**
 * @brief Processa o modo pós-ignição (farol mantém aceso um tempo após desligar)
 * 
 * Quando a ignição desliga, o farol permanece aceso por TEMPO_POS_IGNICAO
 * Isso facilita saída do veículo em locais com pouca luz
 */
void processarPosiIgnicao() {
  if (posIgnicaoAtivo && ativarDrlPosChave) {  // Verifica se DRL pós-chave está ativo
    controlarFarol(true);

    if (agora - timerPosIgnicao >= TEMPO_POS_IGNICAO) {
      posIgnicaoAtivo = false;
      
      // Apaga farol se não houver Follow-Me ativo
      if (!followMeAtivo) {
        controlarFarol(false);
      }
    }
  } else if (posIgnicaoAtivo && !ativarDrlPosChave) {
    // Se desativou o DRL pós-chave, apenas reseta a flag
    posIgnicaoAtivo = false;
    if (!followMeAtivo) {
      controlarFarol(false);
    }
  }
}

/**
 * @brief Processa o relé das setas com bloqueio temporal
 * 
 * Funcionamento:
 * - Liga o relé ao detectar piscada de seta
 * - Mantém ligado por no mínimo TEMPO_MIN_RELE_SETA
 * - Desliga quando: (tempo_mínimo_ok) E (seta_parada_por_TEMPO_SETA_INATIVA)
 */
void processarReleSeta() {
  // Liga o relé ao detectar piscada
  if (setaDebounced && !setaAnterior) {
    ultimaAtividadeSeta = agora;
    
    if (!releSetaAtivo) {
      digitalWrite(PIN_RELE_SETA, HIGH);
      releSetaAtivo = true;
      timerReleSeta = agora;
    }
  }

  // Desliga o relé depois de tempo mínimo E seta inativa
  if (releSetaAtivo) {
    bool tempoMinOk = (agora - timerReleSeta) >= TEMPO_MIN_RELE_SETA;
    bool setaParada = (agora - ultimaAtividadeSeta) >= TEMPO_SETA_INATIVA;

    if (tempoMinOk && setaParada) {
      digitalWrite(PIN_RELE_SETA, LOW);
      releSetaAtivo = false;
    }
  }
}

/**
 * @brief Processa mudanças de estado da ignição
 * 
 * Detecta bordas (transições):
 * - Ligação: reseta Follow-Me, ativa modo DRL
 * - Desligação: ativa modo pós-ignição
 */
void processarIgnicao() {
  // Detecta ligação da ignição (borda de subida)
  if (ignicaoDebounced && !ignicaoAnterior) {
    modoAtivo = true;
    followMeAtivo = false;
    posIgnicaoAtivo = false;
  }

  // Detecta desligação da ignição (borda de descida)
  if (!ignicaoDebounced && ignicaoAnterior) {
    modoAtivo = false;
    timerPosIgnicao = agora;
    posIgnicaoAtivo = true;
  }
}

/**
 * @brief Processa trava/destrava do veículo (ESP32 apenas)
 * Quando detecta uma transição na entrada de trava, liga farol por TEMPO_LUZ_TRAVA
 */
#if SUPORTA_TRAVA
void processarTrava() {
  if (!ativarLuzTrava) return;  // Funcionalidade desativada
  
  // Detecta mudança de estado (trava ou destrava)
  if (travaDebounced != travaAnterior) {
    luzTravaAtiva = true;
    timerLuzTrava = agora;
    controlarFarol(true);
  }
  
  // Desativa a luz após expiração do tempo
  if (luzTravaAtiva && (agora - timerLuzTrava >= TEMPO_LUZ_TRAVA)) {
    luzTravaAtiva = false;
    
    // Apaga farol se não houver outro modo ativo
    if (!modoAtivo && !followMeAtivo && !posIgnicaoAtivo) {
      controlarFarol(false);
    }
  }
}
#endif

/**
 * @brief Fail-safe final: garante farol desligado se nenhum modo ativo
 * Protege contra bugs de lógica e garante desligamento seguro
 */
void failSafeFarol() {
  // Verifica se modoAtivo realmente deve manter farol ligado
  bool drlAtivo = modoAtivo && ativarLuzPosPosChave;
  bool posIgnicaoValido = posIgnicaoAtivo && ativarDrlPosChave;
  
  #if SUPORTA_TRAVA
    if (!drlAtivo && !followMeAtivo && !posIgnicaoValido && !luzTravaAtiva) {
      controlarFarol(false);
    }
  #else
    if (!drlAtivo && !followMeAtivo && !posIgnicaoValido) {
      controlarFarol(false);
    }
  #endif
}

/* ===== FUNÇÕES DE CONFIGURAÇÃO (ESP32) ===== */
#ifdef ESP32

/**
 * @brief Estrutura para armazenar configurações na EEPROM
 */
struct Config {
  unsigned long followMe1;
  unsigned long followMe2;
  unsigned long janelaP;
  unsigned long posPosIgnicao;
  unsigned long tempoMinRele;
  unsigned long tempoSetaInativa;
  unsigned long debounce;
  unsigned long leitura;
  unsigned long tempoLuzTrava;
  bool ativarLuzTrava;
  bool ativarLuzPosPosChave;
  bool ativarFollowMe;
  bool ativarDrlPosChave;
  uint32_t checksum;  // Para validação
};

/**
 * @brief Calcula checksum da configuração
 */
uint32_t calcularChecksum(const Config& cfg) {
  uint32_t sum = cfg.followMe1 + cfg.followMe2 + cfg.janelaP + 
                 cfg.posPosIgnicao + cfg.tempoMinRele + cfg.tempoSetaInativa + 
                 cfg.debounce + cfg.leitura + cfg.tempoLuzTrava +
                 (cfg.ativarLuzTrava ? 1 : 0) + (cfg.ativarLuzPosPosChave ? 1 : 0) +
                 (cfg.ativarFollowMe ? 1 : 0) + (cfg.ativarDrlPosChave ? 1 : 0);
  return sum ^ 0xDEADBEEF;  // XOR com constante para extra validação
}

/**
 * @brief Salva configuração na EEPROM
 */
void salvarConfigEEPROM() {
  Config cfg;
  cfg.followMe1 = TEMPO_FOLLOW_1;
  cfg.followMe2 = TEMPO_FOLLOW_2;
  cfg.janelaP = JANELA_PISCADA;
  cfg.posPosIgnicao = TEMPO_POS_IGNICAO;
  cfg.tempoMinRele = TEMPO_MIN_RELE_SETA;
  cfg.tempoSetaInativa = TEMPO_SETA_INATIVA;
  cfg.debounce = TEMPO_DEBOUNCE;
  cfg.leitura = TEMPO_LEITURA;
  cfg.tempoLuzTrava = TEMPO_LUZ_TRAVA;
  cfg.ativarLuzTrava = ativarLuzTrava;
  cfg.ativarLuzPosPosChave = ativarLuzPosPosChave;
  cfg.ativarFollowMe = ativarFollowMe;
  cfg.ativarDrlPosChave = ativarDrlPosChave;
  cfg.checksum = calcularChecksum(cfg);
  
  EEPROM.writeBytes(EEPROM_ADDR_CONFIG, (uint8_t*)&cfg, sizeof(cfg));
  EEPROM.commit();
}

/**
 * @brief Carrega configuração da EEPROM
 */
void carregarConfigEEPROM() {
  Config cfg;
  EEPROM.readBytes(EEPROM_ADDR_CONFIG, (uint8_t*)&cfg, sizeof(cfg));
  
  // Valida checksum
  if (calcularChecksum(cfg) == cfg.checksum) {
    TEMPO_FOLLOW_1 = cfg.followMe1;
    TEMPO_FOLLOW_2 = cfg.followMe2;
    JANELA_PISCADA = cfg.janelaP;
    TEMPO_POS_IGNICAO = cfg.posPosIgnicao;
    TEMPO_MIN_RELE_SETA = cfg.tempoMinRele;
    TEMPO_SETA_INATIVA = cfg.tempoSetaInativa;
    TEMPO_DEBOUNCE = cfg.debounce;
    TEMPO_LEITURA = cfg.leitura;
    TEMPO_LUZ_TRAVA = cfg.tempoLuzTrava;
    ativarLuzTrava = cfg.ativarLuzTrava;
    ativarLuzPosPosChave = cfg.ativarLuzPosPosChave;
    ativarFollowMe = cfg.ativarFollowMe;
    ativarDrlPosChave = cfg.ativarDrlPosChave;
  }
}

/**
 * @brief Handler: Retorna página HTML
 */
void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

/**
 * @brief Handler: Retorna configurações atuais em JSON
 */
void handleGetConfig() {
  String json = "{";
  json += "\"followMe1\":" + String(TEMPO_FOLLOW_1) + ",";
  json += "\"followMe2\":" + String(TEMPO_FOLLOW_2) + ",";
  json += "\"janelaP\":" + String(JANELA_PISCADA) + ",";
  json += "\"posPosIgnicao\":" + String(TEMPO_POS_IGNICAO) + ",";
  json += "\"tempoMinRele\":" + String(TEMPO_MIN_RELE_SETA) + ",";
  json += "\"tempoSetaInativa\":" + String(TEMPO_SETA_INATIVA) + ",";
  json += "\"debounce\":" + String(TEMPO_DEBOUNCE) + ",";
  json += "\"leitura\":" + String(TEMPO_LEITURA) + ",";
  json += "\"tempoLuzTrava\":" + String(TEMPO_LUZ_TRAVA) + ",";
  json += "\"ativarLuzTrava\":" + String(ativarLuzTrava ? "true" : "false") + ",";
  json += "\"ativarLuzPosPosChave\":" + String(ativarLuzPosPosChave ? "true" : "false") + ",";
  json += "\"ativarFollowMe\":" + String(ativarFollowMe ? "true" : "false") + ",";
  json += "\"ativarDrlPosChave\":" + String(ativarDrlPosChave ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

/**
 * @brief Parse de valor JSON simples - FORWARD DECLARATION
 */
String getValue(String data, String key);

/**
 * @brief Handler: Salva novas configurações
 */
void handleSetConfig() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      
      // Parse JSON simples (sem biblioteca externa)
      if (body.indexOf("followMe1") != -1) {
        TEMPO_FOLLOW_1 = getValue(body, "followMe1").toInt();
        TEMPO_FOLLOW_2 = getValue(body, "followMe2").toInt();
        JANELA_PISCADA = getValue(body, "janelaP").toInt();
        TEMPO_POS_IGNICAO = getValue(body, "posPosIgnicao").toInt();
        TEMPO_MIN_RELE_SETA = getValue(body, "tempoMinRele").toInt();
        TEMPO_SETA_INATIVA = getValue(body, "tempoSetaInativa").toInt();
        TEMPO_DEBOUNCE = getValue(body, "debounce").toInt();
        TEMPO_LEITURA = getValue(body, "leitura").toInt();
        TEMPO_LUZ_TRAVA = getValue(body, "tempoLuzTrava").toInt();
        ativarLuzTrava = getValue(body, "ativarLuzTrava") == "true";
        ativarLuzPosPosChave = getValue(body, "ativarLuzPosPosChave") == "true";
        ativarFollowMe = getValue(body, "ativarFollowMe") == "true";
        ativarDrlPosChave = getValue(body, "ativarDrlPosChave") == "true";
        
        salvarConfigEEPROM();
        
        String response = "{\"success\":true,\"message\":\"Configurações salvas com sucesso\"}";
        server.send(200, "application/json", response);
        return;
      }
    }
  }
  
  String response = "{\"success\":false,\"error\":\"Requisição inválida\"}";
  server.send(400, "application/json", response);
}

/**
 * @brief Handler: Retorna status do dispositivo
 */
void handleStatus() {
  String json = "{";
  json += "\"ssid\":\"" + String(SSID_AP) + "\",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"modoAtivo\":" + String(modoAtivo ? "true" : "false") + ",";
  json += "\"farolLigado\":" + String(farolLigado ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

/**
 * @brief Parse de valor JSON simples
 */
String getValue(String data, String key) {
  int keyPos = data.indexOf(key);
  if (keyPos == -1) return "";
  
  int colonPos = data.indexOf(":", keyPos);
  if (colonPos == -1) return "";
  
  int commaPos = data.indexOf(",", colonPos);
  if (commaPos == -1) commaPos = data.indexOf("}", colonPos);
  
  String value = data.substring(colonPos + 1, commaPos);
  value.trim();
  
  // Remove aspas se existirem
  if (value.startsWith("\"")) value = value.substring(1);
  if (value.endsWith("\"")) value = value.substring(0, value.length() - 1);
  
  return value;
}

/**
 * @brief Inicializa servidor web
 */
void iniciarServidorWeb() {
  // Configurar WiFi em modo AP
  WiFi.softAP(SSID_AP, PASSWORD_AP);
  
  // Rotas do servidor
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handleSetConfig);
  server.on("/api/status", HTTP_GET, handleStatus);
  
  // Inicia servidor
  server.begin();
}

#endif

/* ===== SETUP ===== */
void setup() {
  #ifdef __AVR__
    cli();               // Desabilita interrupções globais
    wdt_reset();
    wdt_disable();       // Garante WDT desligado no boot
  #endif
  
  // Configuração dos pinos
  pinMode(PIN_SETA, INPUT_PULLUP);
  pinMode(PIN_IGNICAO, INPUT_PULLUP);
  pinMode(PIN_FAROL, OUTPUT);
  pinMode(PIN_RELE_SETA, OUTPUT);
  
  #if SUPORTA_TRAVA
    pinMode(PIN_TRAVA, INPUT_PULLUP);
  #endif

  // Inicializa saídas desligadas
  digitalWrite(PIN_FAROL, LOW);
  digitalWrite(PIN_RELE_SETA, LOW);

  #if USE_WATCHDOG
    // Ativa watchdog com timeout de 1 segundo (apenas AVR)
    wdt_enable(WDTO_1S);
  #endif
  
  #ifdef ESP32
    // Inicializa EEPROM e carrega configurações
    EEPROM.begin(EEPROM_SIZE);
    carregarConfigEEPROM();
    
    // Inicia servidor web
    iniciarServidorWeb();
  #endif
  
  #ifdef __AVR__
    sei();               // Reabilita interrupções globais
  #endif
}

/* ===== LOOP PRINCIPAL ===== */
void loop() {
  // Lê timer do sistema
  delay(TEMPO_LEITURA);
  
  #if USE_WATCHDOG
    wdt_reset();         // Alimenta o watchdog (apenas AVR)
  #endif
  
  #ifdef ESP32
    server.handleClient();  // Processa requisições HTTP
  #endif
  
  agora = millis();

  // Lê entradas com debounce
  ignicaoDebounced = lerEntradaDebounced(PIN_IGNICAO, ultimaLeituraIgnicao, ignicaoAnterior);
  setaDebounced = lerEntradaDebounced(PIN_SETA, ultimaLeituraSeta, setaAnterior);
  
  #if SUPORTA_TRAVA
    travaDebounced = lerEntradaDebounced(PIN_TRAVA, ultimaLeituraTrava, travaAnterior);
  #endif

  // Processa lógica de cada subsistema
  processarIgnicao();      // Detecta mudanças de ignição
  processarModoAtivo();    // DRL quando ignição ligada
  processarPosiIgnicao();  // Farol após desligar ignição
  procesarFollowMe();      // Follow-Me com piscadas de seta
  processarReleSeta();     // Controle do relé das setas
  
  #if SUPORTA_TRAVA
    processarTrava();      // Controle de trava/destrava
  #endif

  // Fail-safe: garante desligamento seguro
  failSafeFarol();

  // Atualiza estado anterior das entradas (para detectar bordas no próximo ciclo)
  ignicaoAnterior = ignicaoDebounced;
  setaAnterior = setaDebounced;
  
  #if SUPORTA_TRAVA
    travaAnterior = travaDebounced;
  #endif
}
