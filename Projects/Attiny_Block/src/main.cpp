#include <Arduino.h>

// Set to true during bench tests; set to false for production timings.
static const bool TEST_MODE = true;

// Hardware mapping (ATTiny85 digital pin numbers).
static const uint8_t PIN_DOOR = 0;
static const uint8_t PIN_HANDBRAKE = 1;
static const uint8_t PIN_FUEL_PUMP_LOCK = 2;
static const uint8_t PIN_STATUS_LED = 3;

// Car signals are commonly active LOW (switch to GND).
static const bool DOOR_ACTIVE_LOW = true;
static const bool HANDBRAKE_ACTIVE_LOW = true;

// Output level that activates the fuel pump lock stage/relay.
static const bool FUEL_PUMP_LOCK_ACTIVE_HIGH = true;

// Timings.
static const uint32_t DEBOUNCE_MS = 30;
static const uint32_t BLOCK_DELAY_MS = TEST_MODE ? 10000 : 45000;
static const uint32_t SECRET_TOTAL_WINDOW_MS = TEST_MODE ? 12000 : 7000;
static const uint32_t SECRET_STEP_TIMEOUT_MS = TEST_MODE ? 4000 : 1500;

// Blocked mode: simulate intermittent fuel starvation.
static const uint16_t PUMP_RUN_MIN_MS = 700;
static const uint16_t PUMP_RUN_MAX_MS = 2200;
static const uint16_t PUMP_CUT_MIN_MS = 120;
static const uint16_t PUMP_CUT_MAX_MS = 450;

struct DebouncedInput {
  uint8_t pin;
  bool activeLow;
  bool stableRaw;
  bool lastRaw;
  uint32_t lastChangeMs;
  bool changed;
};

enum SystemState {
  STATE_IDLE,
  STATE_COUNTDOWN,
  STATE_BLOCKED
};

static DebouncedInput doorIn = {PIN_DOOR, DOOR_ACTIVE_LOW, false, false, 0, false};
static DebouncedInput handbrakeIn = {
    PIN_HANDBRAKE, HANDBRAKE_ACTIVE_LOW, false, false, 0, false};

static SystemState state = STATE_IDLE;
static uint32_t countdownStartMs = 0;
static bool ignoreDoorUntilClose = false;

// Secret pattern: engage -> release -> engage -> release -> engage.
static const bool SECRET_PATTERN[] = {true, false, true, false, true};
static const uint8_t SECRET_PATTERN_LEN = sizeof(SECRET_PATTERN) / sizeof(SECRET_PATTERN[0]);
static uint8_t secretIndex = 0;
static uint32_t secretStartMs = 0;
static uint32_t secretLastStepMs = 0;

static bool fuelPumpCutPhase = false;
static uint32_t fuelPumpPhaseUntilMs = 0;
static uint32_t prngState = 0xA53C9E1Fu;
static uint8_t ledPulseCount = 0;
static uint32_t ledPulseUntilMs = 0;

static void scheduleLedPulses(uint8_t count) {
  ledPulseCount = count;
  ledPulseUntilMs = 0;
}

static void updateLedPulseFeedback(uint32_t nowMs) {
  if (ledPulseCount == 0) {
    return;
  }

  if (nowMs < ledPulseUntilMs) {
    return;
  }

  const bool ledNow = (digitalRead(PIN_STATUS_LED) == HIGH);
  digitalWrite(PIN_STATUS_LED, ledNow ? LOW : HIGH);
  ledPulseUntilMs = nowMs + 110;

  if (ledNow) {
    ledPulseCount--;
  }
}

static bool levelFromRaw(bool raw, bool activeLow) {
  return activeLow ? !raw : raw;
}

static bool isActive(const DebouncedInput &in) {
  return levelFromRaw(in.stableRaw, in.activeLow);
}

static void setFuelPumpLock(bool active) {
  const bool outLevel = FUEL_PUMP_LOCK_ACTIVE_HIGH ? active : !active;
  digitalWrite(PIN_FUEL_PUMP_LOCK, outLevel ? HIGH : LOW);
}

static void updateInput(DebouncedInput &in, uint32_t nowMs) {
  const bool raw = digitalRead(in.pin);
  in.changed = false;

  if (raw != in.lastRaw) {
    in.lastRaw = raw;
    in.lastChangeMs = nowMs;
  }

  if ((nowMs - in.lastChangeMs) >= DEBOUNCE_MS && raw != in.stableRaw) {
    in.stableRaw = raw;
    in.changed = true;
  }
}

static uint16_t prngRange(uint16_t minVal, uint16_t maxVal) {
  prngState ^= (prngState << 13);
  prngState ^= (prngState >> 17);
  prngState ^= (prngState << 5);

  const uint16_t span = (maxVal - minVal) + 1;
  return minVal + static_cast<uint16_t>(prngState % span);
}

static void resetSecretSequence() {
  secretIndex = 0;
  secretStartMs = 0;
  secretLastStepMs = 0;
}

static void cancelBlockAndCountdown() {
  state = STATE_IDLE;
  ignoreDoorUntilClose = true;
  fuelPumpCutPhase = false;
  fuelPumpPhaseUntilMs = 0;
  setFuelPumpLock(false);
  resetSecretSequence();
}

static void onSecretStep(bool handbrakeActive, uint32_t nowMs) {
  if (!(state == STATE_COUNTDOWN || state == STATE_BLOCKED)) {
    resetSecretSequence();
    return;
  }

  if (secretIndex == 0) {
    if (handbrakeActive == SECRET_PATTERN[0]) {
      secretIndex = 1;
      secretStartMs = nowMs;
      secretLastStepMs = nowMs;
      scheduleLedPulses(1);
    }
    return;
  }

  if ((nowMs - secretStartMs) > SECRET_TOTAL_WINDOW_MS ||
      (nowMs - secretLastStepMs) > SECRET_STEP_TIMEOUT_MS) {
    resetSecretSequence();
    if (handbrakeActive == SECRET_PATTERN[0]) {
      secretIndex = 1;
      secretStartMs = nowMs;
      secretLastStepMs = nowMs;
    }
    return;
  }

  if (handbrakeActive == SECRET_PATTERN[secretIndex]) {
    secretIndex++;
    secretLastStepMs = nowMs;
    scheduleLedPulses(1);

    if (secretIndex >= SECRET_PATTERN_LEN) {
      cancelBlockAndCountdown();
      // Three quick flashes = sequence accepted.
      scheduleLedPulses(3);
    }
    return;
  }

  if (handbrakeActive == SECRET_PATTERN[0]) {
    secretIndex = 1;
    secretStartMs = nowMs;
    secretLastStepMs = nowMs;
  } else {
    resetSecretSequence();
  }
}

static void updateSecretTimeout(uint32_t nowMs) {
  if (secretIndex == 0) {
    return;
  }

  if ((nowMs - secretStartMs) > SECRET_TOTAL_WINDOW_MS ||
      (nowMs - secretLastStepMs) > SECRET_STEP_TIMEOUT_MS) {
    resetSecretSequence();
  }
}

static void startBlockedMode(uint32_t nowMs) {
  state = STATE_BLOCKED;
  fuelPumpCutPhase = false;
  fuelPumpPhaseUntilMs = nowMs;
}

static void updateFuelPumpFailureSimulation(uint32_t nowMs) {
  if (state != STATE_BLOCKED) {
    return;
  }

  if (nowMs < fuelPumpPhaseUntilMs) {
    return;
  }

  fuelPumpCutPhase = !fuelPumpCutPhase;
  setFuelPumpLock(fuelPumpCutPhase);

  const uint16_t phaseMs = fuelPumpCutPhase
                               ? prngRange(PUMP_CUT_MIN_MS, PUMP_CUT_MAX_MS)
                               : prngRange(PUMP_RUN_MIN_MS, PUMP_RUN_MAX_MS);
  fuelPumpPhaseUntilMs = nowMs + phaseMs;
}

void setup() {
  pinMode(PIN_DOOR, INPUT_PULLUP);
  pinMode(PIN_HANDBRAKE, INPUT_PULLUP);
  pinMode(PIN_FUEL_PUMP_LOCK, OUTPUT);
  pinMode(PIN_STATUS_LED, OUTPUT);

  setFuelPumpLock(false);
  digitalWrite(PIN_STATUS_LED, LOW);

  const uint32_t nowMs = millis();
  doorIn.stableRaw = digitalRead(doorIn.pin);
  doorIn.lastRaw = doorIn.stableRaw;
  doorIn.lastChangeMs = nowMs;

  handbrakeIn.stableRaw = digitalRead(handbrakeIn.pin);
  handbrakeIn.lastRaw = handbrakeIn.stableRaw;
  handbrakeIn.lastChangeMs = nowMs;

  // Add some runtime entropy so each power cycle generates a different fault rhythm.
  prngState ^= micros();
  prngState ^= (static_cast<uint32_t>(digitalRead(PIN_DOOR)) << 1);
  prngState ^= (static_cast<uint32_t>(digitalRead(PIN_HANDBRAKE)) << 2);
}

void loop() {
  const uint32_t nowMs = millis();

  updateInput(doorIn, nowMs);
  updateInput(handbrakeIn, nowMs);

  const bool doorOpen = isActive(doorIn);
  const bool handbrakeActive = isActive(handbrakeIn);

  if (ignoreDoorUntilClose && !doorOpen) {
    ignoreDoorUntilClose = false;
  }

  if (doorIn.changed && doorOpen && !ignoreDoorUntilClose && state == STATE_IDLE) {
    state = STATE_COUNTDOWN;
    countdownStartMs = nowMs;
    resetSecretSequence();
  }

  if (handbrakeIn.changed) {
    onSecretStep(handbrakeActive, nowMs);
  } else {
    updateSecretTimeout(nowMs);
  }

  if (state == STATE_COUNTDOWN && (nowMs - countdownStartMs) >= BLOCK_DELAY_MS) {
    startBlockedMode(nowMs);
  }

  updateFuelPumpFailureSimulation(nowMs);

  // LED behavior: off=idle, blink slow=countdown, blink fast=blocked.
  if (state == STATE_IDLE) {
    if (ledPulseCount == 0) {
      digitalWrite(PIN_STATUS_LED, LOW);
    }
  } else if (state == STATE_COUNTDOWN) {
    if (ledPulseCount == 0) {
      digitalWrite(PIN_STATUS_LED, ((nowMs / 600) % 2) ? HIGH : LOW);
    }
  } else {
    if (ledPulseCount == 0) {
      digitalWrite(PIN_STATUS_LED, ((nowMs / 150) % 2) ? HIGH : LOW);
    }
  }

  updateLedPulseFeedback(nowMs);
}