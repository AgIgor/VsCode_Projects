#include <Arduino.h>

#define DEBUG_SERIAL 0

// Projeto agora focado apenas no Arduino Uno.
// Entradas digitais em logica invertida com pull-up interno:
// D2: trava (normalmente 1, pulso em 0)
// D3: seta (normalmente 1, pulso em 0)
// D4: porta (0 aberta, 1 fechada)
// D5: ignicao (0 ligada, 1 desligada)
// D6: destrava (normalmente 1, pulso em 0)
// A1: LDR simulado por potenciometro

const uint8_t IN_LOCK = 2;
const uint8_t IN_TURN_SIGNAL = 4;
const uint8_t IN_DOOR = 7;
const uint8_t IN_IGNITION = 5;
const uint8_t IN_UNLOCK = 3;
const uint8_t IN_LDR = A1;
const uint8_t OUT_HEADLIGHT = 13;
const uint8_t OUT_ACCESSORY = 12;

struct SystemConfig {
  unsigned long debounceMs;
  unsigned long signalPairWindowMs;
  unsigned long leavingHomeLockMs;
  unsigned long leavingHomeUnlockMs;
  unsigned long courtesyDoorMs;
  unsigned long followMeHomeMs;
  unsigned long courtesyDoorReararmMs;
  unsigned long ambientStableMs;
  unsigned long ldrFaultStableMs;
  unsigned long summaryIntervalMs;
  int ldrDarkThreshold;
  int ldrBrightThreshold;
  int ldrFaultLowRaw;
  int ldrFaultHighRaw;
  bool invertLockInput;
  bool invertTurnSignalInput;
  bool invertDoorInput;
  bool invertIgnitionInput;
  bool invertUnlockInput;
  unsigned long accessoryOnMs;
};

constexpr SystemConfig CONFIG = {
    30UL,
    1500UL,
    40000UL,
    20000UL,
    20000UL,
    30000UL,
    5000UL,
    2500UL,
    4000UL,
    5000UL,
    790,
    750,
    5,
    1018,
    true,
    true,
    false,
    true,
    true,
    600000UL};

struct DebouncedInput {
  uint8_t pin;
  uint8_t stable;
  uint8_t rawPrev;
  unsigned long changedAt;
  bool invert;
};

struct TimedMode {
  bool active;
  unsigned long startedAt;
  unsigned long duration;
};

enum ModeSource {
  MODE_NONE = 0,
  MODE_AUTO_DRIVE,
  MODE_LEAVING_HOME,
  MODE_COURTESY_DOOR,
  MODE_FOLLOW_ME_HOME
};

DebouncedInput inLock = {IN_LOCK, HIGH, HIGH, 0, false};
DebouncedInput inTurn = {IN_TURN_SIGNAL, HIGH, HIGH, 0, false};
DebouncedInput inDoor = {IN_DOOR, HIGH, HIGH, 0, false};
DebouncedInput inIgnition = {IN_IGNITION, HIGH, HIGH, 0, false};
DebouncedInput inUnlock = {IN_UNLOCK, HIGH, HIGH, 0, false};

TimedMode leavingHomeTimer = {false, 0, CONFIG.leavingHomeLockMs};
TimedMode courtesyDoorTimer = {false, 0, CONFIG.courtesyDoorMs};
TimedMode followMeHomeTimer = {false, 0, CONFIG.followMeHomeMs};
TimedMode accessoryTimer = {false, 0, CONFIG.accessoryOnMs};
bool accessoryDoorCyclePending = false;

bool lockPulseEvent = false;
bool unlockPulseEvent = false;
bool turnPulseEvent = false;
bool doorOpenEvent = false;
bool doorCloseEvent = false;
bool ignitionOnEvent = false;
bool ignitionOffEvent = false;

unsigned long lastLockPulseAt = 0;
unsigned long lastUnlockPulseAt = 0;
unsigned long lastTurnPulseAt = 0;
bool lockPulsePending = false;
bool unlockPulsePending = false;
bool turnPulsePending = false;

bool ambientInitialized = false;
bool ambientDark = false;
bool ambientPendingDark = false;
unsigned long ambientChangedAt = 0;
int ambientLdrRaw = 0;

bool autoDriveActive = false;
bool autoDriveActivePrev = false;
bool courtesyDoorArmed = false;
bool courtesyDoorReararmPending = false;
unsigned long courtesyDoorClosedAt = 0;
bool ldrFaultActive = false;
bool ldrFaultPending = false;
unsigned long ldrFaultChangedAt = 0;

const __FlashStringHelper *modeName(ModeSource mode) {
  switch (mode) {
    case MODE_AUTO_DRIVE:
      return F("AUTO_DRIVE");
    case MODE_LEAVING_HOME:
      return F("LEAVING_HOME");
    case MODE_COURTESY_DOOR:
      return F("COURTESY_DOOR");
    case MODE_FOLLOW_ME_HOME:
      return F("FOLLOW_ME_HOME");
    default:
      return F("NONE");
  }
}

const __FlashStringHelper *nextActionForMode(ModeSource mode) {
  switch (mode) {
    case MODE_AUTO_DRIVE:
      return F("manter farol ligado enquanto o ambiente continuar escuro");
    case MODE_LEAVING_HOME:
      return F("iluminar a aproximacao pelo tempo configurado para trava ou destrava");
    case MODE_COURTESY_DOOR:
      return F("iluminar a abertura de porta por 20s");
    case MODE_FOLLOW_ME_HOME:
      return F("manter iluminacao apos desligar a ignicao por 30s");
    default:
      return F("aguardar novo evento de entrada ou mudanca de luminosidade");
  }
}

#if DEBUG_SERIAL
void debugPrefix(unsigned long now) {
  Serial.print('[');
  Serial.print(now);
  Serial.print(F(" ms] "));
}

void debugLog(unsigned long now, const __FlashStringHelper *message) {
  debugPrefix(now);
  Serial.println(message);
}

void debugLogEvent(
    unsigned long now,
    const __FlashStringHelper *eventMessage,
    const __FlashStringHelper *nextAction) {
  debugPrefix(now);
  Serial.print(F("Evento: "));
  Serial.println(eventMessage);
  Serial.print(F("             Proxima: "));
  Serial.println(nextAction);
}

void debugLogAmbient(unsigned long now, const __FlashStringHelper *stateMessage) {
  debugPrefix(now);
  Serial.print(F("Ambiente: "));
  Serial.print(stateMessage);
  Serial.print(F(" (A1="));
  Serial.print(ambientLdrRaw);
  Serial.print(F(", dark<="));
  Serial.print(CONFIG.ldrDarkThreshold);
  Serial.print(F(", bright>="));
  Serial.print(CONFIG.ldrBrightThreshold);
  Serial.print(F(", fault="));
  Serial.print(ldrFaultActive ? F("ON") : F("OFF"));
  Serial.println(')');
}

void debugLogModeStart(
    unsigned long now,
    ModeSource mode,
    const __FlashStringHelper *reason,
    bool renewed,
    unsigned long durationMs) {
  debugPrefix(now);
  Serial.print(F("Modo "));
  Serial.print(modeName(mode));
  Serial.print(renewed ? F(": renovado") : F(": iniciado"));
  Serial.print(F(" por "));
  Serial.print(durationMs / 1000UL);
  Serial.println(F("s"));
  Serial.print(F("             Motivo: "));
  Serial.println(reason);
  Serial.print(F("             Proxima: "));
  Serial.println(nextActionForMode(mode));
}

void debugLogModeStop(
    unsigned long now,
    ModeSource mode,
    const __FlashStringHelper *reason) {
  debugPrefix(now);
  Serial.print(F("Modo "));
  Serial.print(modeName(mode));
  Serial.println(F(": encerrado"));
  Serial.print(F("             Motivo: "));
  Serial.println(reason);
  Serial.print(F("             Proxima: "));
  Serial.println(nextActionForMode(MODE_NONE));
}
#else
inline void debugPrefix(unsigned long) {}
inline void debugLog(unsigned long, const __FlashStringHelper *) {}
inline void debugLogEvent(
    unsigned long,
    const __FlashStringHelper *,
    const __FlashStringHelper *) {}
inline void debugLogAmbient(unsigned long, const __FlashStringHelper *) {}
inline void debugLogModeStart(
    unsigned long,
    ModeSource,
    const __FlashStringHelper *,
    bool,
    unsigned long) {}
inline void debugLogModeStop(
    unsigned long,
    ModeSource,
    const __FlashStringHelper *) {}
#endif

unsigned long remainingTimerMs(const TimedMode &timer, unsigned long now) {
  if (!timer.active) {
    return 0;
  }

  const unsigned long elapsed = now - timer.startedAt;
  return elapsed >= timer.duration ? 0 : timer.duration - elapsed;
}

uint8_t normalizeRawInput(uint8_t raw, bool invert) {
  return invert ? (raw == HIGH ? LOW : HIGH) : raw;
}

void debugLogTimerProgress(
    unsigned long now,
    ModeSource mode,
    const TimedMode &timer,
    long &lastReportedSeconds) {
  const unsigned long remainingMs = remainingTimerMs(timer, now);
  if (!timer.active || remainingMs == 0) {
    lastReportedSeconds = -1;
    return;
  }

  const long remainingSeconds = (long)((remainingMs + 999UL) / 1000UL);
  if (remainingSeconds == lastReportedSeconds) {
    return;
  }

  debugPrefix(now);
  Serial.print(F("Timer "));
  Serial.print(modeName(mode));
  Serial.print(F(": faltam "));
  Serial.print(remainingSeconds);
  Serial.println(F("s"));

  lastReportedSeconds = remainingSeconds;
}

long timerRemainingSeconds(const TimedMode &timer, unsigned long now) {
  const unsigned long remainingMs = remainingTimerMs(timer, now);
  return remainingMs == 0 ? 0L : (long)((remainingMs + 999UL) / 1000UL);
}

bool updateInput(DebouncedInput &input, unsigned long now) {
  const uint8_t raw = normalizeRawInput(digitalRead(input.pin), input.invert);

  if (raw != input.rawPrev) {
    input.rawPrev = raw;
    input.changedAt = now;
  }

  if ((now - input.changedAt) >= CONFIG.debounceMs && input.stable != input.rawPrev) {
    input.stable = input.rawPrev;
    return true;
  }

  return false;
}

bool isPulseActive(const DebouncedInput &input) {
  return input.stable == LOW;
}

bool isDoorOpen() {
  return inDoor.stable == LOW;
}

bool isDoorClosed() {
  return !isDoorOpen();
}

bool isIgnitionOn() {
  return inIgnition.stable == LOW;
}

void clearSignalMatches() {
  lockPulsePending = false;
  unlockPulsePending = false;
  turnPulsePending = false;
}

void expireSignalMatchWindow(unsigned long now) {
  if (lockPulsePending && (now - lastLockPulseAt) > CONFIG.signalPairWindowMs) {
    lockPulsePending = false;
  }

  if (unlockPulsePending && (now - lastUnlockPulseAt) > CONFIG.signalPairWindowMs) {
    unlockPulsePending = false;
  }

  if (turnPulsePending && (now - lastTurnPulseAt) > CONFIG.signalPairWindowMs) {
    turnPulsePending = false;
  }
}

enum SignalMatchType {
  SIGNAL_MATCH_NONE = 0,
  SIGNAL_MATCH_LOCK,
  SIGNAL_MATCH_UNLOCK
};

unsigned long pulseDelta(unsigned long a, unsigned long b) {
  return a > b ? a - b : b - a;
}

SignalMatchType consumeSignalMatch(unsigned long now) {
  expireSignalMatchWindow(now);

  const bool lockMatched =
      lockPulsePending && turnPulsePending &&
      pulseDelta(lastLockPulseAt, lastTurnPulseAt) <= CONFIG.signalPairWindowMs;
  const bool unlockMatched =
      unlockPulsePending && turnPulsePending &&
      pulseDelta(lastUnlockPulseAt, lastTurnPulseAt) <= CONFIG.signalPairWindowMs;

  if (!lockMatched && !unlockMatched) {
    return SIGNAL_MATCH_NONE;
  }

  if (lockMatched && unlockMatched) {
    if (pulseDelta(lastUnlockPulseAt, lastTurnPulseAt) <
        pulseDelta(lastLockPulseAt, lastTurnPulseAt)) {
      unlockPulsePending = false;
      turnPulsePending = false;
      return SIGNAL_MATCH_UNLOCK;
    }

    lockPulsePending = false;
    turnPulsePending = false;
    return SIGNAL_MATCH_LOCK;
  }

  if (lockMatched) {
    lockPulsePending = false;
    turnPulsePending = false;
    return SIGNAL_MATCH_LOCK;
  }

  unlockPulsePending = false;
  turnPulsePending = false;
  return SIGNAL_MATCH_UNLOCK;
}

void startOrExtendTimer(
    TimedMode &timer,
    ModeSource mode,
    unsigned long now,
    const __FlashStringHelper *reason) {
  const bool renewed = timer.active && remainingTimerMs(timer, now) > 0;
  timer.active = true;
  timer.startedAt = now;
  debugLogModeStart(now, mode, reason, renewed, timer.duration);
}

void stopTimer(
    TimedMode &timer,
    ModeSource mode,
    unsigned long now,
    const __FlashStringHelper *reason) {
  if (!timer.active) {
    return;
  }

  timer.active = false;
  debugLogModeStop(now, mode, reason);
}

#if DEBUG_SERIAL
void debugLogAccessoryStart(unsigned long now, const __FlashStringHelper *reason, bool renewed) {
  debugPrefix(now);
  Serial.print(F("Acessorios: "));
  Serial.print(renewed ? F("renovado") : F("ativado"));
  Serial.print(F(" por "));
  Serial.print(accessoryTimer.duration / 1000UL);
  Serial.println(F("s"));
  Serial.print(F("             Motivo: "));
  Serial.println(reason);
  Serial.print(F("             Proxima: "));
  Serial.println(F("desligar apos porta abrir/fechar ou tempo expirar"));
}

void debugLogAccessoryStop(unsigned long now, const __FlashStringHelper *reason) {
  debugPrefix(now);
  Serial.println(F("Acessorios: desligando"));
  Serial.print(F("             Motivo: "));
  Serial.println(reason);
}
#else
inline void debugLogAccessoryStart(unsigned long, const __FlashStringHelper *, bool) {}
inline void debugLogAccessoryStop(unsigned long, const __FlashStringHelper *) {}
#endif

void startAccessoryTimer(unsigned long now, const __FlashStringHelper *reason) {
  const bool renewed = accessoryTimer.active && remainingTimerMs(accessoryTimer, now) > 0;
  accessoryTimer.active = true;
  accessoryTimer.startedAt = now;
  accessoryDoorCyclePending = false;
  debugLogAccessoryStart(now, reason, renewed);
}

void stopAccessoryTimer(unsigned long now, const __FlashStringHelper *reason) {
  if (!accessoryTimer.active) {
    return;
  }

  accessoryTimer.active = false;
  accessoryDoorCyclePending = false;
  debugLogAccessoryStop(now, reason);
}

void cancelAllTimedModes(unsigned long now, const __FlashStringHelper *reason) {
  stopTimer(leavingHomeTimer, MODE_LEAVING_HOME, now, reason);
  stopTimer(courtesyDoorTimer, MODE_COURTESY_DOOR, now, reason);
  stopTimer(followMeHomeTimer, MODE_FOLLOW_ME_HOME, now, reason);
  stopAccessoryTimer(now, F("ignicao ligada; acessorios desligando"));
}

void expireTimers(unsigned long now) {
  if (leavingHomeTimer.active && remainingTimerMs(leavingHomeTimer, now) == 0) {
    stopTimer(leavingHomeTimer, MODE_LEAVING_HOME, now, F("temporizador de Leaving Home encerrado"));
  }

  if (courtesyDoorTimer.active && remainingTimerMs(courtesyDoorTimer, now) == 0) {
    stopTimer(courtesyDoorTimer, MODE_COURTESY_DOOR, now, F("temporizador de 20s encerrado"));
  }

  if (followMeHomeTimer.active && remainingTimerMs(followMeHomeTimer, now) == 0) {
    stopTimer(followMeHomeTimer, MODE_FOLLOW_ME_HOME, now, F("temporizador de 30s encerrado"));
  }

  if (accessoryTimer.active && remainingTimerMs(accessoryTimer, now) == 0) {
    stopAccessoryTimer(now, F("temporizador de acessorios expirado"));
  }
}

void updateCourtesyDoorArm(unsigned long now) {
  if (doorOpenEvent) {
    courtesyDoorArmed = false;
    courtesyDoorReararmPending = false;
    return;
  }

  if (doorCloseEvent) {
    courtesyDoorReararmPending = true;
    courtesyDoorClosedAt = now;
    return;
  }

  if (courtesyDoorReararmPending && isDoorClosed() &&
      (now - courtesyDoorClosedAt) >= CONFIG.courtesyDoorReararmMs) {
    courtesyDoorReararmPending = false;
    courtesyDoorArmed = true;
    debugLogEvent(
        now,
        F("anti-rearme da porta liberado"),
        F("proxima abertura pode acionar Courtesy Door"));
  }
}

void updateLdrFaultState(unsigned long now) {
  const bool faultCandidate =
      ambientLdrRaw <= CONFIG.ldrFaultLowRaw || ambientLdrRaw >= CONFIG.ldrFaultHighRaw;

  if (faultCandidate != ldrFaultPending) {
    ldrFaultPending = faultCandidate;
    ldrFaultChangedAt = now;
    return;
  }

  if (faultCandidate == ldrFaultActive || (now - ldrFaultChangedAt) < CONFIG.ldrFaultStableMs) {
    return;
  }

  ldrFaultActive = faultCandidate;
  if (ldrFaultActive) {
    debugLogEvent(
        now,
        F("falha no LDR detectada"),
        F("bloquear modos dependentes de luminosidade ate a leitura voltar ao normal"));
  } else {
    ambientChangedAt = now;
    debugLogEvent(
        now,
        F("LDR voltou a faixa valida"),
        F("retomar avaliacao automatica de claro e escuro"));
  }
}

void initAmbientState(unsigned long now) {
  ambientLdrRaw = analogRead(IN_LDR);
  ambientDark = ambientLdrRaw >= ((CONFIG.ldrDarkThreshold + CONFIG.ldrBrightThreshold) / 2);
  ambientPendingDark = ambientDark;
  ambientChangedAt = now;
  ambientInitialized = true;
  ldrFaultPending =
      ambientLdrRaw <= CONFIG.ldrFaultLowRaw || ambientLdrRaw >= CONFIG.ldrFaultHighRaw;
  ldrFaultChangedAt = now;
  debugLogAmbient(now, ambientDark ? F("ESCURO inicial") : F("CLARO inicial"));
}

void updateAmbientState(unsigned long now) {
  ambientLdrRaw = analogRead(IN_LDR);

  if (!ambientInitialized) {
    initAmbientState(now);
    return;
  }

  updateLdrFaultState(now);
  if (ldrFaultActive) {
    return;
  }

  bool targetDark = ambientDark;
  if (!ambientDark && ambientLdrRaw >= CONFIG.ldrDarkThreshold) {
    targetDark = true;
  } else if (ambientDark && ambientLdrRaw <= CONFIG.ldrBrightThreshold) {
    targetDark = false;
  }

  if (targetDark == ambientDark) {
    ambientPendingDark = ambientDark;
    return;
  }

  if (targetDark != ambientPendingDark) {
    ambientPendingDark = targetDark;
    ambientChangedAt = now;
    return;
  }

  if ((now - ambientChangedAt) < CONFIG.ambientStableMs) {
    return;
  }

  ambientDark = targetDark;
  ambientPendingDark = targetDark;
  debugLogAmbient(now, ambientDark ? F("ESCURO estavel") : F("CLARO estavel"));
}

void readInputsAndEvents(unsigned long now) {
  lockPulseEvent = false;
  unlockPulseEvent = false;
  turnPulseEvent = false;
  doorOpenEvent = false;
  doorCloseEvent = false;
  ignitionOnEvent = false;
  ignitionOffEvent = false;

  if (updateInput(inLock, now) && isPulseActive(inLock)) {
    lockPulseEvent = true;
  }

  if (updateInput(inUnlock, now) && isPulseActive(inUnlock)) {
    unlockPulseEvent = true;
  }

  if (updateInput(inTurn, now) && isPulseActive(inTurn)) {
    turnPulseEvent = true;
  }

  if (updateInput(inDoor, now)) {
    if (isDoorOpen()) {
      doorOpenEvent = true;
    } else {
      doorCloseEvent = true;
    }
  }

  if (updateInput(inIgnition, now)) {
    if (isIgnitionOn()) {
      ignitionOnEvent = true;
    } else {
      ignitionOffEvent = true;
    }
  }
}

void processLogic(unsigned long now) {
  const bool ignitionOn = isIgnitionOn();
  const bool ignitionOff = !ignitionOn;
  const bool doorClosed = isDoorClosed();
  const bool ambientUsableDark = ambientDark && !ldrFaultActive;
  const bool leavingHomeEligible = ignitionOff && doorClosed && ambientUsableDark;
  SignalMatchType signalMatch = SIGNAL_MATCH_NONE;

  updateCourtesyDoorArm(now);

  if (lockPulseEvent) {
    debugLogEvent(
        now,
        F("pulso detectado em trava"),
        leavingHomeEligible
            ? F("aguardar pulso de seta proximo para validar Leaving Home")
            : ignitionOn ? F("ignorado porque a ignicao esta ligada")
                         : ambientUsableDark ? F("ignorado porque a porta esta aberta")
                                       : F("ignorado porque o ambiente esta claro"));
  }

  if (unlockPulseEvent) {
    debugLogEvent(
        now,
        F("pulso detectado em destrava"),
        leavingHomeEligible
            ? F("aguardar pulso de seta proximo para validar destrava")
            : ignitionOn ? F("ignorado porque a ignicao esta ligada")
                         : ambientUsableDark ? F("ignorado porque a porta esta aberta")
                                             : F("ignorado porque o ambiente esta claro"));
  }

  if (turnPulseEvent) {
    debugLogEvent(
        now,
        F("pulso detectado em seta"),
        leavingHomeEligible
            ? F("aguardar pulso de trava proximo para validar Leaving Home")
            : ignitionOn ? F("ignorado porque a ignicao esta ligada")
                         : ambientUsableDark ? F("ignorado porque a porta esta aberta")
                                       : F("ignorado porque o ambiente esta claro"));
  }

  if (doorOpenEvent) {
    if (accessoryTimer.active) {
      accessoryDoorCyclePending = true;
      debugLogEvent(
          now,
          F("porta abriu"),
          F("acessorios permanecem ligados ate a porta fechar ou 2 min"));
    } else {
      debugLogEvent(
          now,
          F("porta abriu"),
                  ignitionOff && ambientUsableDark && courtesyDoorArmed
              ? F("iniciar ou renovar Courtesy Door por 20s")
                    : ignitionOff && ambientUsableDark
                        ? F("ignorado pelo anti-rearme; aguardar porta fechada por alguns segundos")
              : ignitionOn ? F("ignorado porque a ignicao esta ligada")
                           : F("ignorado porque o ambiente esta claro"));
    }
  }

  if (doorCloseEvent) {
    if (accessoryTimer.active && accessoryDoorCyclePending) {
      stopAccessoryTimer(now, F("porta abriu e fechou apos ignicao desligada"));
    }

    debugLogEvent(
        now,
        F("porta fechou"),
        ignitionOff
            ? F("habilitar validacao de Leaving Home quando houver ambiente escuro")
            : F("modo AUTO continua monitorando a luminosidade"));
  }

  if (ignitionOnEvent) {
    debugLogEvent(
        now,
        F("ignicao ligada"),
        ambientUsableDark ? F("modo AUTO pode manter os farois ligados")
                    : F("modo AUTO aguardando ambiente escurecer"));
    cancelAllTimedModes(now, F("ignicao ligada; modo automatico assumiu o controle"));
    clearSignalMatches();
  }

  if (ignitionOffEvent) {
    debugLogEvent(
        now,
        F("ignicao desligada"),
        ambientUsableDark ? F("avaliar Follow Me Home e modos de cortesia")
                    : F("aguardar evento noturno"));
    startAccessoryTimer(
        now,
        F("ignicao desligada; manter alimentacao de acessorios por 2 min ou ate porta abrir/fechar"));
  }

  if (!leavingHomeEligible) {
    clearSignalMatches();
  } else {
    expireSignalMatchWindow(now);

    if (lockPulseEvent) {
      lastLockPulseAt = now;
      lockPulsePending = true;
    }

    if (unlockPulseEvent) {
      lastUnlockPulseAt = now;
      unlockPulsePending = true;
    }

    if (turnPulseEvent) {
      lastTurnPulseAt = now;
      turnPulsePending = true;
    }

    signalMatch = consumeSignalMatch(now);

    if (signalMatch == SIGNAL_MATCH_LOCK) {
      debugLogEvent(
          now,
          F("trava e seta confirmados dentro da janela"),
          F("iniciar ou renovar Leaving Home por 40s"));
        leavingHomeTimer.duration = CONFIG.leavingHomeLockMs;
      startOrExtendTimer(
          leavingHomeTimer,
          MODE_LEAVING_HOME,
          now,
          F("par de sinais de trava e seta detectado com ambiente escuro"));
    } else if (signalMatch == SIGNAL_MATCH_UNLOCK) {
      debugLogEvent(
          now,
          F("destrava e seta confirmados dentro da janela"),
          F("iniciar ou renovar Leaving Home por 20s"));
        leavingHomeTimer.duration = CONFIG.leavingHomeUnlockMs;
      startOrExtendTimer(
          leavingHomeTimer,
          MODE_LEAVING_HOME,
          now,
          F("par de sinais de destrava e seta detectado com ambiente escuro"));
    }
  }

  if (ignitionOff && ambientUsableDark && doorOpenEvent && courtesyDoorArmed) {
    startOrExtendTimer(
        courtesyDoorTimer,
        MODE_COURTESY_DOOR,
        now,
        F("porta abriu com ignicao desligada e ambiente escuro"));
  }

  if (ignitionOffEvent && ambientUsableDark && autoDriveActivePrev) {
    startOrExtendTimer(
        followMeHomeTimer,
        MODE_FOLLOW_ME_HOME,
        now,
        F("ignicao desligada enquanto o modo AUTO estava ativo"));
  }

  expireTimers(now);
  autoDriveActive = ignitionOn && ambientUsableDark;
  autoDriveActivePrev = autoDriveActive;
}

ModeSource primaryOutputSource() {
  if (autoDriveActive) {
    return MODE_AUTO_DRIVE;
  }

  if (followMeHomeTimer.active) {
    return MODE_FOLLOW_ME_HOME;
  }

  if (leavingHomeTimer.active) {
    return MODE_LEAVING_HOME;
  }

  if (courtesyDoorTimer.active) {
    return MODE_COURTESY_DOOR;
  }

  return MODE_NONE;
}

#if DEBUG_SERIAL
void debugLogAccessoryTimerProgress(unsigned long now) {
  static long lastAccessorySeconds = -1;
  const unsigned long remainingMs = remainingTimerMs(accessoryTimer, now);
  if (!accessoryTimer.active || remainingMs == 0) {
    lastAccessorySeconds = -1;
    return;
  }

  const long remainingSeconds = (long)((remainingMs + 999UL) / 1000UL);
  if (remainingSeconds == lastAccessorySeconds) {
    return;
  }

  debugPrefix(now);
  Serial.print(F("Timer ACESSORIOS: faltam "));
  Serial.print(remainingSeconds);
  Serial.println(F("s"));
  lastAccessorySeconds = remainingSeconds;
}
#else
inline void debugLogAccessoryTimerProgress(unsigned long) {}
#endif

void debugLogActiveTimers(unsigned long now) {
  static long lastLeavingSeconds = -1;
  static long lastCourtesySeconds = -1;
  static long lastFollowSeconds = -1;

  debugLogTimerProgress(now, MODE_LEAVING_HOME, leavingHomeTimer, lastLeavingSeconds);
  debugLogTimerProgress(now, MODE_COURTESY_DOOR, courtesyDoorTimer, lastCourtesySeconds);
  debugLogTimerProgress(now, MODE_FOLLOW_ME_HOME, followMeHomeTimer, lastFollowSeconds);
  debugLogAccessoryTimerProgress(now);
}

#if DEBUG_SERIAL
void debugLogPeriodicSummary(unsigned long now) {
  static unsigned long lastSummaryAt = 0;

  if ((now - lastSummaryAt) < CONFIG.summaryIntervalMs) {
    return;
  }

  lastSummaryAt = now;

  debugPrefix(now);
  Serial.print(F("Resumo: A1="));
  Serial.print(ambientLdrRaw);
  Serial.print(F(" ambient="));
  if (ldrFaultActive) {
    Serial.print(F("FAULT"));
  } else {
    Serial.print(ambientDark ? F("ESCURO") : F("CLARO"));
  }
  Serial.print(F(" ign="));
  Serial.print(isIgnitionOn() ? F("ON") : F("OFF"));
  Serial.print(F(" porta="));
  Serial.print(isDoorOpen() ? F("ABERTA") : F("FECHADA"));
  Serial.print(F(" cortesia="));
  Serial.print(courtesyDoorArmed ? F("ARMADA") : F("BLOQUEADA"));
  Serial.print(F(" acessorios="));
  Serial.print(accessoryTimer.active ? F("ON") : F("OFF"));
  Serial.print(F(" saida="));
  Serial.print(primaryOutputSource() == MODE_NONE ? F("OFF") : F("ON"));
  Serial.print(F(" modo="));
  Serial.print(modeName(primaryOutputSource()));
  Serial.print(F(" tLH="));
  Serial.print(timerRemainingSeconds(leavingHomeTimer, now));
  Serial.print(F("s tCD="));
  Serial.print(timerRemainingSeconds(courtesyDoorTimer, now));
  Serial.print(F("s tFMH="));
  Serial.print(timerRemainingSeconds(followMeHomeTimer, now));
  Serial.println(F("s"));
}
#else
inline void debugLogPeriodicSummary(unsigned long) {}
#endif

void updateOutput(unsigned long now) {
  static bool outputPrev = false;
  static ModeSource primaryPrev = MODE_NONE;
  static bool accessoryPrev = false;

  const bool outputOn =
      autoDriveActive || leavingHomeTimer.active || courtesyDoorTimer.active || followMeHomeTimer.active;
  const ModeSource primaryMode = primaryOutputSource();
  const bool accessoryOn = isIgnitionOn() || accessoryTimer.active;

  digitalWrite(OUT_HEADLIGHT, outputOn ? HIGH : LOW);
  digitalWrite(OUT_ACCESSORY, accessoryOn ? HIGH : LOW);

#if DEBUG_SERIAL
  if (outputOn != outputPrev) {
    debugPrefix(now);
    Serial.print(F("Saida do farol: "));
    Serial.print(outputOn ? F("LIGADA") : F("DESLIGADA"));
    if (outputOn) {
      Serial.print(F(" por "));
      Serial.println(modeName(primaryMode));
      Serial.print(F("             Proxima: "));
      Serial.println(nextActionForMode(primaryMode));
    } else {
      Serial.println();
      Serial.print(F("             Proxima: "));
      Serial.println(nextActionForMode(MODE_NONE));
    }
  } else if (outputOn && primaryMode != primaryPrev) {
    debugPrefix(now);
    Serial.print(F("Saida do farol mantida por "));
    Serial.println(modeName(primaryMode));
    Serial.print(F("             Proxima: "));
    Serial.println(nextActionForMode(primaryMode));
  }

  if (accessoryOn != accessoryPrev) {
    debugPrefix(now);
    Serial.print(F("Saida de acessorios: "));
    Serial.println(accessoryOn ? F("LIGADA") : F("DESLIGADA"));
  }
#endif

  outputPrev = outputOn;
  primaryPrev = primaryMode;
  accessoryPrev = accessoryOn;
}

void setup() {
#if DEBUG_SERIAL
  Serial.begin(9600);
#endif

  pinMode(IN_LOCK, INPUT_PULLUP);
  pinMode(IN_TURN_SIGNAL, INPUT_PULLUP);
  pinMode(IN_DOOR, INPUT_PULLUP);
  pinMode(IN_IGNITION, INPUT_PULLUP);
  pinMode(IN_UNLOCK, INPUT_PULLUP);

  pinMode(OUT_HEADLIGHT, OUTPUT);
  digitalWrite(OUT_HEADLIGHT, LOW);
  pinMode(OUT_ACCESSORY, OUTPUT);
  digitalWrite(OUT_ACCESSORY, LOW);

  const unsigned long now = millis();

  inLock.invert = CONFIG.invertLockInput;
  inTurn.invert = CONFIG.invertTurnSignalInput;
  inDoor.invert = CONFIG.invertDoorInput;
  inIgnition.invert = CONFIG.invertIgnitionInput;
  inUnlock.invert = CONFIG.invertUnlockInput;

  inLock.stable = normalizeRawInput(digitalRead(IN_LOCK), inLock.invert);
  inLock.rawPrev = inLock.stable;
  inLock.changedAt = now;

  inTurn.stable = normalizeRawInput(digitalRead(IN_TURN_SIGNAL), inTurn.invert);
  inTurn.rawPrev = inTurn.stable;
  inTurn.changedAt = now;

  inDoor.stable = normalizeRawInput(digitalRead(IN_DOOR), inDoor.invert);
  inDoor.rawPrev = inDoor.stable;
  inDoor.changedAt = now;

  inIgnition.stable = normalizeRawInput(digitalRead(IN_IGNITION), inIgnition.invert);
  inIgnition.rawPrev = inIgnition.stable;
  inIgnition.changedAt = now;

  inUnlock.stable = normalizeRawInput(digitalRead(IN_UNLOCK), inUnlock.invert);
  inUnlock.rawPrev = inUnlock.stable;
  inUnlock.changedAt = now;

  initAmbientState(now);
    courtesyDoorArmed = isDoorClosed();
    courtesyDoorReararmPending = false;
    courtesyDoorClosedAt = now;
    autoDriveActive = isIgnitionOn() && ambientDark && !ldrFaultActive;
  autoDriveActivePrev = autoDriveActive;

  debugLog(now, F("Debug serial habilitado para Arduino Uno com LDR em A1."));
  debugLogEvent(
      now,
      F("modos ativos: AUTO, Leaving Home, Courtesy Door e Follow Me Home"),
      nextActionForMode(MODE_NONE));
    debugLogEvent(
      now,
      F("recursos extras: anti-rearme da porta, fail-safe do LDR e resumo periodico"),
      F("ajustar CONFIG no topo do arquivo se precisar mudar tempos e limiares"));
}

void loop() {
  const unsigned long now = millis();

  updateAmbientState(now);
  readInputsAndEvents(now);
  processLogic(now);
  debugLogActiveTimers(now);
  updateOutput(now);
  debugLogPeriodicSummary(now);
}