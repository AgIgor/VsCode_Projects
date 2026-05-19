#include <Arduino.h>

// Entradas digitais com pullup: sinais são ativos em LOW.
static const uint8_t PIN_LOCK     = 2;   // pulso de trava
static const uint8_t PIN_UNLOCK   = 3;   // pulso de destrava
static const uint8_t PIN_TURN     = 4;   // pulso de seta
static const uint8_t PIN_IGNITION = 5;   // ignição ligado/desligado
static const uint8_t PIN_DOOR     = 6;   // porta aberta/fechada
static const uint8_t PIN_LDR      = A1;  // sensor LDR invertido
static const uint8_t PIN_HEAD    = 13;  // saída de farol

static const unsigned long DEBOUNCE_MS        = 30;
static const unsigned long MODE_WINDOW_MS      = 4000;
static const unsigned long COMFORT_DURATION_MS = 25000;
static const unsigned long DOOR_HOLD_MS        = 15000;
static const unsigned long LDR_STABLE_MS       = 500;
static const unsigned long STATUS_PRINT_MS     = 500;

static const int LDR_THRESHOLD_CLEAR = 750; // < 750 = claro
static const int LDR_THRESHOLD_DARK  = 790; // > 790 = escuro

struct DebouncedInput {
  uint8_t pin;
  bool stableState;
  bool rawState;
  unsigned long lastBounce;
};

static DebouncedInput inputLock     = {PIN_LOCK, HIGH, HIGH, 0};
static DebouncedInput inputUnlock   = {PIN_UNLOCK, HIGH, HIGH, 0};
static DebouncedInput inputTurn     = {PIN_TURN, HIGH, HIGH, 0};
static DebouncedInput inputIgnition = {PIN_IGNITION, HIGH, HIGH, 0};
static DebouncedInput inputDoor     = {PIN_DOOR, HIGH, HIGH, 0};

static bool ambientDark = false;
static bool ldrCandidate = false;
static unsigned long ldrCandidateStartMs = 0;
static unsigned long lastStatusMs = 0;

static bool waitingForTurnAfterLock = false;
static unsigned long pendingModeStartMs = 0;
static unsigned long comfortModeEndMs = 0;
static unsigned long doorLightEndMs = 0;

static unsigned long eventCountLock = 0;
static unsigned long eventCountUnlock = 0;
static unsigned long eventCountTurn = 0;

bool readDebouncedInput(DebouncedInput &input, unsigned long now) {
  bool raw = digitalRead(input.pin);
  if (raw != input.rawState) {
    input.rawState = raw;
    input.lastBounce = now;
  }

  if (raw != input.stableState && (now - input.lastBounce) >= DEBOUNCE_MS) {
    input.stableState = raw;
    return true;
  }

  return false;
}

bool inputActive(const DebouncedInput &input) {
  return input.stableState == LOW;
}

void activateComfortMode(unsigned long now) {
  comfortModeEndMs = now + COMFORT_DURATION_MS;
  waitingForTurnAfterLock = false;
}

void printStatus(unsigned long now) {
  if (now - lastStatusMs < STATUS_PRINT_MS) {
    return;
  }
  lastStatusMs = now;

  int ldrValue = analogRead(PIN_LDR);
  bool ignitionOn = inputActive(inputIgnition);
  bool doorOpen = inputActive(inputDoor);
  bool lockActive = inputActive(inputLock);
  bool unlockActive = inputActive(inputUnlock);
  bool turnActive = inputActive(inputTurn);
  bool isDark = ambientDark;
  bool comfortActive = now < comfortModeEndMs;
  bool doorHoldActive = now < doorLightEndMs;
  unsigned long pendingRemaining = 0;
  if (waitingForTurnAfterLock) {
    unsigned long elapsed = now - pendingModeStartMs;
    pendingRemaining = elapsed < MODE_WINDOW_MS ? MODE_WINDOW_MS - elapsed : 0;
  }

  unsigned long ldrDelay = 0;
  if (ldrCandidate != ambientDark) {
    unsigned long diff = now - ldrCandidateStartMs;
    ldrDelay = diff < LDR_STABLE_MS ? LDR_STABLE_MS - diff : 0;
  }

  Serial.println(F("=== DEBUG ILUMINACAO ==="));
  Serial.print(F("LOCK: ")); Serial.print(lockActive ? F("ON") : F("OFF"));
  Serial.print(F("  UNLOCK: ")); Serial.print(unlockActive ? F("ON") : F("OFF"));
  Serial.print(F("  TURN: ")); Serial.println(turnActive ? F("ON") : F("OFF"));
  Serial.print(F("IGN: ")); Serial.print(ignitionOn ? F("ON") : F("OFF"));
  Serial.print(F("  DOOR: ")); Serial.print(doorOpen ? F("OPEN") : F("CLOSED"));
  Serial.print(F("  LDR: ")); Serial.print(ldrValue);
  Serial.print(F("  AMBIENTE: ")); Serial.println(isDark ? F("ESCURO") : F("CLARO"));
  Serial.print(F("COMFORT: ")); Serial.print(comfortActive ? F("SIM") : F("NAO"));
  Serial.print(F("  DOOR-HOLD: ")); Serial.print(doorHoldActive ? F("SIM") : F("NAO"));
  Serial.print(F("  PENDING: ")); Serial.println(waitingForTurnAfterLock ? pendingRemaining : 0);
  Serial.print(F("TIMERS (ms): COMFORT=")); Serial.print(comfortActive ? comfortModeEndMs - now : 0);
  Serial.print(F(" DOOR=")); Serial.print(doorHoldActive ? doorLightEndMs - now : 0);
  Serial.print(F(" LDR-DELAY=")); Serial.println(ldrDelay);
  Serial.print(F("EVENTOS: LOCK=")); Serial.print(eventCountLock);
  Serial.print(F(" UNLOCK=")); Serial.print(eventCountUnlock);
  Serial.print(F(" TURN=")); Serial.println(eventCountTurn);
  Serial.println();
}

void setup() {
  pinMode(PIN_LOCK, INPUT_PULLUP);
  pinMode(PIN_UNLOCK, INPUT_PULLUP);
  pinMode(PIN_TURN, INPUT_PULLUP);
  pinMode(PIN_IGNITION, INPUT_PULLUP);
  pinMode(PIN_DOOR, INPUT_PULLUP);
  pinMode(PIN_HEAD, OUTPUT);
  digitalWrite(PIN_HEAD, LOW);

  pinMode(PIN_LDR, INPUT);

  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  Serial.println(F("Sistema de iluminacao automotiva inicializado"));
  Serial.println(F("Entradas: 2=trava,3=destrava,4=seta,5=ignicao,6=porta | Saida: 13"));
}

void loop() {
  unsigned long now = millis();

  bool changedLock = readDebouncedInput(inputLock, now);
  bool changedUnlock = readDebouncedInput(inputUnlock, now);
  bool changedTurn = readDebouncedInput(inputTurn, now);
  readDebouncedInput(inputIgnition, now);
  bool changedDoor = readDebouncedInput(inputDoor, now);

  bool lockActive = inputActive(inputLock);
  bool unlockActive = inputActive(inputUnlock);
  bool turnActive = inputActive(inputTurn);
  bool ignitionOn = inputActive(inputIgnition);
  bool doorOpen = inputActive(inputDoor);

  if (changedLock && lockActive) {
    eventCountLock++;
    waitingForTurnAfterLock = true;
    pendingModeStartMs = now;
    Serial.println(F("Evento: trava detectada, aguardando seta para modo de conforto"));
  }

  if (changedUnlock && unlockActive) {
    eventCountUnlock++;
    waitingForTurnAfterLock = true;
    pendingModeStartMs = now;
    Serial.println(F("Evento: destrava detectada, aguardando seta para modo de conforto"));
  }

  if (changedTurn && turnActive) {
    eventCountTurn++;
    Serial.println(F("Evento: seta detectada"));
    if (waitingForTurnAfterLock && (now - pendingModeStartMs) <= MODE_WINDOW_MS) {
      activateComfortMode(now);
      Serial.println(F("Modo conforto ativado por sequencia trava/destrava + seta"));
    }
  }

  if (waitingForTurnAfterLock && (now - pendingModeStartMs) > MODE_WINDOW_MS) {
    waitingForTurnAfterLock = false;
    Serial.println(F("Janela de modo expirou sem sinal de seta"));
  }

  int ldrValue = analogRead(PIN_LDR);
  bool newDark = ambientDark;
  if (ldrValue < LDR_THRESHOLD_CLEAR) {
    newDark = false;
  } else if (ldrValue > LDR_THRESHOLD_DARK) {
    newDark = true;
  }

  if (newDark != ldrCandidate) {
    ldrCandidate = newDark;
    ldrCandidateStartMs = now;
  }

  if (ldrCandidate != ambientDark && (now - ldrCandidateStartMs) >= LDR_STABLE_MS) {
    ambientDark = ldrCandidate;
    Serial.print(F("Mudanca de ambiente estabilizada: "));
    Serial.println(ambientDark ? F("ESCURO") : F("CLARO"));
  }

  if (changedDoor && doorOpen) {
    doorLightEndMs = now + DOOR_HOLD_MS;
    Serial.println(F("Porta aberta: iluminacao de conforto habilitada por tempo"));
  }

  bool comfortActive = now < comfortModeEndMs;
  bool doorHoldActive = now < doorLightEndMs;

  bool headlightOn = false;
  if (comfortActive) {
    headlightOn = true;
  } else if (ignitionOn && ambientDark) {
    headlightOn = true;
  } else if (doorHoldActive && ambientDark) {
    headlightOn = true;
  }

  digitalWrite(PIN_HEAD, headlightOn ? HIGH : LOW);

  printStatus(now);
}
