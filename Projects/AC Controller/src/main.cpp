#include <Arduino.h>

// ========== CONFIGURAÇÃO DOS PINOS ==========
#define PIN_MAP_SENSOR      A1    // PB2 - Sensor MAP (analógico)
#define PIN_TPS_SENSOR      A2    // PB4 - Sensor TPS (analógico)
#define PIN_INJECTOR        PB3   // PB3 - Sinal do bico injetor (digital)
#define PIN_AC_RELAY        PB1   // PB1 - Relé do compressor AC (HIGH = ligado)

// ========== PARÂMETROS DO MOTOR ==========
#define MOTOR_CYLINDERS     4     // Motor 4 cilindros
#define RPM_LOW_THRESHOLD   2000  // RPM abaixo deste valor é considerado baixo
#define RPM_HIGH_THRESHOLD  4500  // RPM acima deste valor é considerado alto

// ========== THRESHOLDS DE CARGA ==========
#define TPS_HIGH_THRESHOLD  70    // TPS acima de 70% indica aceleração forte
#define MAP_HIGH_THRESHOLD  75    // MAP acima de 75% indica alta carga
#define MAP_LOW_THRESHOLD   40    // MAP abaixo de 40% indica baixa carga

// ========== PARÂMETROS DE TEMPORIZAÇÃO ==========
#define AC_CUTOFF_DELAY     200   // Tempo em ms antes de cortar o AC
#define AC_RESTORE_DELAY    1000  // Tempo em ms antes de religar o AC
#define INJECTOR_TIMEOUT    150   // Timeout para leitura do injetor (ms)

// ========== VARIÁVEIS GLOBAIS ==========
volatile unsigned long injectorPulseStart = 0;
volatile unsigned long injectorPulseWidth = 0;
volatile unsigned long lastInjectorPulse = 0;
volatile uint16_t pulseCount = 0;

unsigned long lastRpmCalc = 0;
uint16_t currentRPM = 0;
uint16_t currentPulseWidth = 0;

bool acEnabled = true;
unsigned long acStateChangeTime = 0;
bool acCutoffRequested = false;

// ========== INTERRUPÇÃO DO INJETOR ==========
void injectorISR() {
  unsigned long now = micros();
  
  if (digitalRead(PIN_INJECTOR) == HIGH) {
    // Início do pulso
    injectorPulseStart = now;
  } else {
    // Fim do pulso
    if (injectorPulseStart > 0) {
      injectorPulseWidth = now - injectorPulseStart;
      lastInjectorPulse = millis();
      pulseCount++;
    }
  }
}

// ========== SETUP ==========
void setup() {
  // Configuração dos pinos
  pinMode(PIN_MAP_SENSOR, INPUT);
  pinMode(PIN_TPS_SENSOR, INPUT);
  pinMode(PIN_INJECTOR, INPUT);
  pinMode(PIN_AC_RELAY, OUTPUT);
  
  // Inicializa com AC ligado
  digitalWrite(PIN_AC_RELAY, HIGH);
  acEnabled = true;
  
  // Configura interrupção para leitura do injetor
  attachInterrupt(digitalPinToInterrupt(PIN_INJECTOR), injectorISR, CHANGE);
  
  lastRpmCalc = millis();
}

// ========== FUNÇÃO: LER SENSORES ==========
void readSensors(uint8_t &mapValue, uint8_t &tpsValue) {
  // Lê MAP (0-100%)
  int mapRaw = analogRead(PIN_MAP_SENSOR);
  mapValue = map(mapRaw, 0, 1023, 0, 100);
  
  // Lê TPS (0-100%)
  int tpsRaw = analogRead(PIN_TPS_SENSOR);
  tpsValue = map(tpsRaw, 0, 1023, 0, 100);
}

// ========== FUNÇÃO: CALCULAR RPM ==========
void calculateRPM() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastRpmCalc;
  
  // Calcula RPM a cada 100ms
  if (elapsed >= 100) {
    if (pulseCount > 0) {
      // RPM = (pulsos * 60000) / (tempo_ms * cilindros / 2)
      // Para motor 4 tempos: cada cilindro injeta uma vez a cada 2 rotações
      currentRPM = (pulseCount * 60000UL) / (elapsed * (MOTOR_CYLINDERS / 2));
      
      // Atualiza largura de pulso (converte de micros para ms)
      currentPulseWidth = injectorPulseWidth / 1000;
    } else {
      currentRPM = 0;
    }
    
    pulseCount = 0;
    lastRpmCalc = now;
  }
  
  // Verifica timeout do sinal do injetor
  if (millis() - lastInjectorPulse > INJECTOR_TIMEOUT) {
    currentRPM = 0;
  }
}

// ========== FUNÇÃO: DETERMINAR SE DEVE CORTAR AC ==========
bool shouldCutOffAC(uint8_t mapValue, uint8_t tpsValue, uint16_t rpm) {
  // Condição 1: Aceleração forte (TPS alto) + RPM baixo
  bool strongAcceleration = (tpsValue > TPS_HIGH_THRESHOLD) && (rpm < RPM_LOW_THRESHOLD);
  
  // Condição 2: Carga alta (MAP alto) + RPM baixo (subidas, arrancadas)
  bool highLoadLowRPM = (mapValue > MAP_HIGH_THRESHOLD) && (rpm < RPM_LOW_THRESHOLD);
  
  // Condição 3: Motor em marcha lenta ou muito baixo RPM (abaixo de 800 RPM)
  bool veryLowRPM = rpm < 800;
  
  return strongAcceleration || highLoadLowRPM || veryLowRPM;
}

// ========== FUNÇÃO: DETERMINAR SE DEVE RESTAURAR AC ==========
bool shouldRestoreAC(uint8_t mapValue, uint8_t tpsValue, uint16_t rpm) {
  // Restaura AC quando condições voltarem ao normal
  bool normalAcceleration = (tpsValue < (TPS_HIGH_THRESHOLD - 10));
  bool normalLoad = (mapValue < (MAP_HIGH_THRESHOLD - 10));
  bool adequateRPM = (rpm > (RPM_LOW_THRESHOLD + 300));
  
  return normalAcceleration && normalLoad && adequateRPM;
}

// ========== FUNÇÃO: CONTROLAR RELÉ DO AC ==========
void controlACRelay() {
  unsigned long now = millis();
  
  if (acCutoffRequested && !acEnabled) {
    // AC já está desligado, nada a fazer
    return;
  }
  
  if (acCutoffRequested && acEnabled) {
    // Verifica se passou o delay para cortar
    if (now - acStateChangeTime >= AC_CUTOFF_DELAY) {
      digitalWrite(PIN_AC_RELAY, LOW);
      acEnabled = false;
    }
  }
  
  if (!acCutoffRequested && !acEnabled) {
    // Verifica se passou o delay para restaurar
    if (now - acStateChangeTime >= AC_RESTORE_DELAY) {
      digitalWrite(PIN_AC_RELAY, HIGH);
      acEnabled = true;
    }
  }
}

// ========== LOOP PRINCIPAL ==========
void loop() {
  uint8_t mapValue, tpsValue;
  
  // Lê sensores
  readSensors(mapValue, tpsValue);
  
  // Calcula RPM
  calculateRPM();
  
  // Determina se deve cortar ou restaurar o AC
  bool cutoff = shouldCutOffAC(mapValue, tpsValue, currentRPM);
  
  if (cutoff != acCutoffRequested) {
    // Mudança de estado solicitada
    acCutoffRequested = cutoff;
    acStateChangeTime = millis();
  }
  
  // Se não há cutoff requisitado, verifica se pode restaurar
  if (!acCutoffRequested && !acEnabled) {
    if (shouldRestoreAC(mapValue, tpsValue, currentRPM)) {
      // Condições OK para restaurar
    } else {
      // Ainda não pode restaurar, continua esperando
      acCutoffRequested = false; // Garante que não vai tentar religar prematuramente
    }
  }
  
  // Controla o relé do AC
  controlACRelay();
  
  // Pequeno delay para não sobrecarregar o ATtiny85
  delay(20);
}