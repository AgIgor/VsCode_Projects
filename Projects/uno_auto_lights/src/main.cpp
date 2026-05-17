#include <Arduino.h>

// Entradas digitais com PULLUP
constexpr uint8_t PIN_LOCK = 2;      // pulso de trava
constexpr uint8_t PIN_UNLOCK = 3;    // pulso de destrava
constexpr uint8_t PIN_TURN = 4;      // pulso de seta
constexpr uint8_t PIN_IGNITION = 5;  // ignição on/off
constexpr uint8_t PIN_DOOR = 6;      // porta aberta/fechada
constexpr uint8_t PIN_HEADLIGHT = 13; // saída farol
constexpr uint8_t PIN_LDR = A1;      // LDR

// Temporizações
constexpr unsigned long DEBOUNCE_MS = 50;
constexpr unsigned long TURN_WINDOW_MS = 3000;
constexpr unsigned long COMFORT_LIGHT_MS = 15000;
constexpr unsigned long DOOR_COURTESY_MS = 10000;
constexpr unsigned long DEBUG_INTERVAL_MS = 1000;
constexpr unsigned long LDR_SAMPLE_INTERVAL_MS = 100;

// LDR invertido e com histérese para evitar piscadas
constexpr uint16_t LDR_DARK_THRESHOLD = 600;
constexpr uint16_t LDR_BRIGHT_THRESHOLD = 440;
constexpr uint8_t LDR_SAMPLES = 8;

enum LightMode {
  MODE_NONE = 0,
  MODE_ENTRY,
  MODE_EXIT,
};

// Estados de entrada estabilizados
bool lockStable = false;
bool unlockStable = false;
bool turnStable = false;
bool ignitionStable = false;
bool doorStable = false;

// Bounces raw
bool lockRaw = false;
bool unlockRaw = false;
bool turnRaw = false;
bool ignitionRaw = false;
bool doorRaw = false;

unsigned long lockDebounceTime = 0;
unsigned long unlockDebounceTime = 0;
unsigned long turnDebounceTime = 0;
unsigned long ignitionDebounceTime = 0;
unsigned long doorDebounceTime = 0;

// Contadores e debug
uint32_t lockPulseCount = 0;
uint32_t unlockPulseCount = 0;
uint32_t turnPulseCount = 0;
uint32_t ignitionChangeCount = 0;
uint32_t doorChangeCount = 0;
uint32_t modeActivateCount = 0;
uint32_t ldrSampleCount = 0;
uint32_t debugPrintCount = 0;

// Modo tiro com seta após trava/destrava
LightMode pendingMode = MODE_NONE;
unsigned long pendingModeExpiry = 0;
LightMode activeMode = MODE_NONE;
unsigned long activeModeExpiry = 0;

// LDR suave
uint16_t ldrSamples[LDR_SAMPLES] = {0};
uint8_t ldrSampleIndex = 0;
uint32_t ldrSampleSum = 0;
bool darkState = false;
unsigned long lastLdrSampleMs = 0;

// Debug
unsigned long lastDebugMs = 0;
uint32_t loopCount = 0;
unsigned long doorCourtesyExpiry = 0;
unsigned long doorOpenedAt = 0;

bool readActive(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

inline uint32_t min32(uint32_t a, uint32_t b) {
  return (a < b) ? a : b;
}

bool updateStableState(uint8_t pin, bool &rawState, bool &stableState, unsigned long &debounceTime, unsigned long now) {
  bool raw = readActive(pin);
  if (raw != rawState) {
    debounceTime = now;
    rawState = raw;
  }
  if ((now - debounceTime) >= DEBOUNCE_MS) {
    if (raw != stableState) {
      stableState = raw;
      return true;
    }
  }
  return false;
}

void handlePendingMode(unsigned long now) {
  if (pendingMode != MODE_NONE && now > pendingModeExpiry) {
    pendingMode = MODE_NONE;
    pendingModeExpiry = 0;
  }
}

void processLockPulse(unsigned long now) {
  pendingMode = MODE_EXIT;
  pendingModeExpiry = now + TURN_WINDOW_MS;
}

void processUnlockPulse(unsigned long now) {
  pendingMode = MODE_ENTRY;
  pendingModeExpiry = now + TURN_WINDOW_MS;
}

void processTurnPulse(unsigned long now) {
  if (pendingMode != MODE_NONE && now <= pendingModeExpiry) {
    activeMode = pendingMode;
    activeModeExpiry = now + COMFORT_LIGHT_MS;
    modeActivateCount++;
    pendingMode = MODE_NONE;
    pendingModeExpiry = 0;
  }
}

uint16_t readLdrAverage() {
  uint16_t raw = analogRead(PIN_LDR);
  uint16_t inverted = 1023 - raw;
  if (ldrSampleCount < LDR_SAMPLES) {
    ldrSampleSum += inverted;
    ldrSamples[ldrSampleIndex++] = inverted;
    ldrSampleCount++;
  } else {
    ldrSampleSum -= ldrSamples[ldrSampleIndex];
    ldrSampleSum += inverted;
    ldrSamples[ldrSampleIndex] = inverted;
    ldrSampleIndex = (ldrSampleIndex + 1) % LDR_SAMPLES;
  }
  if (ldrSampleCount == 0) {
    return inverted;
  }
  return ldrSampleSum / min32(ldrSampleCount, LDR_SAMPLES);
}

void updateLdrState(unsigned long now) {
  if ((now - lastLdrSampleMs) < LDR_SAMPLE_INTERVAL_MS) {
    return;
  }
  lastLdrSampleMs = now;
  uint16_t average = readLdrAverage();

  if (darkState) {
    if (average <= LDR_BRIGHT_THRESHOLD) {
      darkState = false;
    }
  } else {
    if (average >= LDR_DARK_THRESHOLD) {
      darkState = true;
    }
  }
}

void printDebug(unsigned long now) {
  if ((now - lastDebugMs) < DEBUG_INTERVAL_MS) {
    return;
  }
  lastDebugMs = now;
  debugPrintCount++;

  uint16_t raw = analogRead(PIN_LDR);
  uint16_t inverted = 1023 - raw;
  uint16_t average = (ldrSampleCount == 0) ? inverted : (ldrSampleSum / min32(ldrSampleCount, LDR_SAMPLES));

  Serial.println(F("--- DEBUG ---"));
  Serial.print(F("Tempo (ms): ")); Serial.println(now);
  Serial.print(F("Loop count: ")); Serial.println(loopCount);
  Serial.print(F("Lock pulses: ")); Serial.println(lockPulseCount);
  Serial.print(F("Unlock pulses: ")); Serial.println(unlockPulseCount);
  Serial.print(F("Turn pulses: ")); Serial.println(turnPulseCount);
  Serial.print(F("Ignition changes: ")); Serial.println(ignitionChangeCount);
  Serial.print(F("Door changes: ")); Serial.println(doorChangeCount);
  Serial.print(F("Mode ativa: "));
  if (activeMode == MODE_ENTRY) Serial.println(F("ENTRY"));
  else if (activeMode == MODE_EXIT) Serial.println(F("EXIT"));
  else Serial.println(F("NONE"));
  Serial.print(F("Mode expiry: ")); Serial.println(activeModeExpiry > now ? activeModeExpiry - now : 0);
  Serial.print(F("Pending mode: "));
  if (pendingMode == MODE_ENTRY) Serial.println(F("ENTRY"));
  else if (pendingMode == MODE_EXIT) Serial.println(F("EXIT"));
  else Serial.println(F("NONE"));
  Serial.print(F("Pending expiry: ")); Serial.println(pendingModeExpiry > now ? pendingModeExpiry - now : 0);
  Serial.print(F("Ignition: ")); Serial.println(ignitionStable ? F("ON") : F("OFF"));
  Serial.print(F("Door: ")); Serial.println(doorStable ? F("OPEN") : F("CLOSED"));
  Serial.print(F("Dark: ")); Serial.println(darkState ? F("SIM") : F("NAO"));
  Serial.print(F("LDR raw: ")); Serial.print(raw);
  Serial.print(F(" inv: ")); Serial.print(inverted);
  Serial.print(F(" avg: ")); Serial.println(average);
  Serial.print(F("Headlight: ")); Serial.println(digitalRead(PIN_HEADLIGHT) == HIGH ? F("ON") : F("OFF"));
  Serial.println();
}

void setup() {
  pinMode(PIN_LOCK, INPUT_PULLUP);
  pinMode(PIN_UNLOCK, INPUT_PULLUP);
  pinMode(PIN_TURN, INPUT_PULLUP);
  pinMode(PIN_IGNITION, INPUT_PULLUP);
  pinMode(PIN_DOOR, INPUT_PULLUP);
  pinMode(PIN_HEADLIGHT, OUTPUT);
  digitalWrite(PIN_HEADLIGHT, LOW);

  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  Serial.println(F("Sistema de iluminacao inteligente inicializado"));
  Serial.println(F("Entradas: 2=trava,3=destrava,4=seta,5=ignicao,6=porta; Saida:13 farol; LDR=A1"));
}

void loop() {
  unsigned long now = millis();
  loopCount++;

  if (updateStableState(PIN_LOCK, lockRaw, lockStable, lockDebounceTime, now)) {
    if (lockStable) {
      lockPulseCount++;
      processLockPulse(now);
    }
  }
  if (updateStableState(PIN_UNLOCK, unlockRaw, unlockStable, unlockDebounceTime, now)) {
    if (unlockStable) {
      unlockPulseCount++;
      processUnlockPulse(now);
    }
  }
  if (updateStableState(PIN_TURN, turnRaw, turnStable, turnDebounceTime, now)) {
    if (turnStable) {
      turnPulseCount++;
      processTurnPulse(now);
    }
  }
  if (updateStableState(PIN_IGNITION, ignitionRaw, ignitionStable, ignitionDebounceTime, now)) {
    ignitionChangeCount++;
  }
  if (updateStableState(PIN_DOOR, doorRaw, doorStable, doorDebounceTime, now)) {
    doorChangeCount++;
    if (doorStable) {
      doorOpenedAt = now;
      doorCourtesyExpiry = now + DOOR_COURTESY_MS;
    } else {
      doorOpenedAt = 0;
    }
  }

  handlePendingMode(now);
  updateLdrState(now);

  bool headlightOn = false;
  if (activeMode != MODE_NONE && now <= activeModeExpiry) {
    headlightOn = true;
  } else if (ignitionStable && darkState) {
    headlightOn = true;
  } else if (doorCourtesyExpiry > now && darkState && !ignitionStable) {
    headlightOn = true;
  }

  digitalWrite(PIN_HEADLIGHT, headlightOn ? HIGH : LOW);
  printDebug(now);
}
