/*
 * main.c — STM8 port da logica de iluminacao automatica
 *
 * Pinos de entrada e saida sao definidos na secao "Pin assignments"
 * abaixo. Altere os valores de acordo com o hardware antes de compilar.
 *
 * Logica portada de uno.cpp (Arduino Uno) para C puro (SDCC/STM8).
 * Partes de debug serial foram removidas.
 */

#include <Arduino.h>

/* ===========================================================
 * Pin assignments — altere para os pinos corretos do hardware
 * =========================================================== */
#define IN_LOCK          PB5   /* TODO: pino da trava         */
#define IN_UNLOCK        PB5   /* TODO: pino da destrava      */
#define IN_TURN_SIGNAL   PD1   /* TODO: pino da seta          */

#define IN_IGNITION      PB0   /* TODO: pino da ignicao       */
#define IN_DOOR          PB1   /* TODO: pino da porta         */

#define IN_LDR           PF4   /* TODO: pino analogico do LDR */

#define OUT_HEADLIGHT    PD3   /* TODO: pino do farol         */
#define OUT_ACCESSORY    PC1   /* TODO: pino dos acessorios   */

/* ===========================================================
 * Configuracoes de tempo e limiar
 * =========================================================== */
#define CFG_DEBOUNCE_MS              30UL
#define CFG_SIGNAL_PAIR_WINDOW_MS    1500UL
#define CFG_LEAVING_HOME_LOCK_MS     40000UL
#define CFG_LEAVING_HOME_UNLOCK_MS   20000UL
#define CFG_COURTESY_DOOR_MS         20000UL
#define CFG_FOLLOW_ME_HOME_MS        30000UL
#define CFG_COURTESY_DOOR_REARM_MS   5000UL
#define CFG_AMBIENT_STABLE_MS        2500UL
#define CFG_LDR_FAULT_STABLE_MS      4000UL
#define CFG_LDR_DARK_THRESHOLD       790
#define CFG_LDR_BRIGHT_THRESHOLD     750
#define CFG_LDR_FAULT_LOW_RAW        5
#define CFG_LDR_FAULT_HIGH_RAW       1018
#define CFG_INVERT_LOCK              1   /* 1 = logica invertida (pull-up, pulso em LOW) */
#define CFG_INVERT_TURN_SIGNAL       1
#define CFG_INVERT_DOOR              0   /* 0 = LOW = aberta                             */
#define CFG_INVERT_IGNITION          1
#define CFG_INVERT_UNLOCK            1
#define CFG_ACCESSORY_ON_MS          600000UL

/* ===========================================================
 * Tipos
 * =========================================================== */
typedef uint8_t bool_t;

typedef struct {
    uint8_t       pin;
    uint8_t       stable;
    uint8_t       rawPrev;
    unsigned long changedAt;
    bool_t        invert;
} DebouncedInput;

typedef struct {
    bool_t        active;
    unsigned long startedAt;
    unsigned long duration;
} TimedMode;

typedef enum {
    SIGNAL_MATCH_NONE = 0,
    SIGNAL_MATCH_LOCK,
    SIGNAL_MATCH_UNLOCK
} SignalMatchType;

/* ===========================================================
 * Variaveis de estado
 * =========================================================== */
static DebouncedInput inLock     = {IN_LOCK,        HIGH, HIGH, 0, CFG_INVERT_LOCK};
static DebouncedInput inTurn     = {IN_TURN_SIGNAL, HIGH, HIGH, 0, CFG_INVERT_TURN_SIGNAL};
static DebouncedInput inDoor     = {IN_DOOR,        HIGH, HIGH, 0, CFG_INVERT_DOOR};
static DebouncedInput inIgnition = {IN_IGNITION,    HIGH, HIGH, 0, CFG_INVERT_IGNITION};
static DebouncedInput inUnlock   = {IN_UNLOCK,      HIGH, HIGH, 0, CFG_INVERT_UNLOCK};

static TimedMode leavingHomeTimer  = {FALSE, 0, CFG_LEAVING_HOME_LOCK_MS};
static TimedMode courtesyDoorTimer = {FALSE, 0, CFG_COURTESY_DOOR_MS};
static TimedMode followMeHomeTimer = {FALSE, 0, CFG_FOLLOW_ME_HOME_MS};
static TimedMode accessoryTimer    = {FALSE, 0, CFG_ACCESSORY_ON_MS};
static bool_t accessoryDoorCyclePending = FALSE;

static bool_t lockPulseEvent   = FALSE;
static bool_t unlockPulseEvent = FALSE;
static bool_t turnPulseEvent   = FALSE;
static bool_t doorOpenEvent    = FALSE;
static bool_t doorCloseEvent   = FALSE;
static bool_t ignitionOnEvent  = FALSE;
static bool_t ignitionOffEvent = FALSE;

static unsigned long lastLockPulseAt   = 0;
static unsigned long lastUnlockPulseAt = 0;
static unsigned long lastTurnPulseAt   = 0;
static bool_t lockPulsePending   = FALSE;
static bool_t unlockPulsePending = FALSE;
static bool_t turnPulsePending   = FALSE;

static bool_t        ambientInitialized  = FALSE;
static bool_t        ambientDark         = FALSE;
static bool_t        ambientPendingDark  = FALSE;
static unsigned long ambientChangedAt    = 0;
static int           ambientLdrRaw       = 0;

static bool_t        autoDriveActive          = FALSE;
static bool_t        autoDriveActivePrev      = FALSE;
static bool_t        courtesyDoorArmed        = FALSE;
static bool_t        courtesyDoorReararmPending = FALSE;
static unsigned long courtesyDoorClosedAt     = 0;
static bool_t        ldrFaultActive           = FALSE;
static bool_t        ldrFaultPending          = FALSE;
static unsigned long ldrFaultChangedAt        = 0;

/* ===========================================================
 * Funcoes auxiliares
 * =========================================================== */
static unsigned long remainingTimerMs(const TimedMode *t, unsigned long now)
{
    unsigned long elapsed;
    if (!t->active) return 0;
    elapsed = now - t->startedAt;
    return elapsed >= t->duration ? 0 : t->duration - elapsed;
}

static uint8_t normalizeRawInput(uint8_t raw, bool_t invert)
{
    return invert ? (raw == HIGH ? LOW : HIGH) : raw;
}

static bool_t updateInput(DebouncedInput *inp, unsigned long now)
{
    uint8_t raw = normalizeRawInput(digitalRead(inp->pin), inp->invert);
    if (raw != inp->rawPrev) {
        inp->rawPrev   = raw;
        inp->changedAt = now;
    }
    if ((now - inp->changedAt) >= CFG_DEBOUNCE_MS && inp->stable != inp->rawPrev) {
        inp->stable = inp->rawPrev;
        return TRUE;
    }
    return FALSE;
}

static bool_t isPulseActive(const DebouncedInput *inp) { return inp->stable == LOW; }
static bool_t isDoorOpen(void)    { return inDoor.stable    == LOW; }
static bool_t isDoorClosed(void)  { return !isDoorOpen(); }
static bool_t isIgnitionOn(void)  { return inIgnition.stable == LOW; }

static void clearSignalMatches(void)
{
    lockPulsePending = unlockPulsePending = turnPulsePending = FALSE;
}

static void expireSignalMatchWindow(unsigned long now)
{
    if (lockPulsePending   && (now - lastLockPulseAt)   > CFG_SIGNAL_PAIR_WINDOW_MS) lockPulsePending   = FALSE;
    if (unlockPulsePending && (now - lastUnlockPulseAt) > CFG_SIGNAL_PAIR_WINDOW_MS) unlockPulsePending = FALSE;
    if (turnPulsePending   && (now - lastTurnPulseAt)   > CFG_SIGNAL_PAIR_WINDOW_MS) turnPulsePending   = FALSE;
}

static unsigned long pulseDelta(unsigned long a, unsigned long b)
{
    return a > b ? a - b : b - a;
}

static SignalMatchType consumeSignalMatch(unsigned long now)
{
    bool_t lockMatched, unlockMatched;
    expireSignalMatchWindow(now);

    lockMatched   = lockPulsePending   && turnPulsePending &&
                    pulseDelta(lastLockPulseAt,   lastTurnPulseAt) <= CFG_SIGNAL_PAIR_WINDOW_MS;
    unlockMatched = unlockPulsePending && turnPulsePending &&
                    pulseDelta(lastUnlockPulseAt, lastTurnPulseAt) <= CFG_SIGNAL_PAIR_WINDOW_MS;

    if (!lockMatched && !unlockMatched) return SIGNAL_MATCH_NONE;

    if (lockMatched && unlockMatched) {
        if (pulseDelta(lastUnlockPulseAt, lastTurnPulseAt) <
            pulseDelta(lastLockPulseAt,   lastTurnPulseAt)) {
            unlockPulsePending = turnPulsePending = FALSE;
            return SIGNAL_MATCH_UNLOCK;
        }
        lockPulsePending = turnPulsePending = FALSE;
        return SIGNAL_MATCH_LOCK;
    }

    if (lockMatched) {
        lockPulsePending = turnPulsePending = FALSE;
        return SIGNAL_MATCH_LOCK;
    }

    unlockPulsePending = turnPulsePending = FALSE;
    return SIGNAL_MATCH_UNLOCK;
}

static void startOrExtendTimer(TimedMode *t, unsigned long now)
{
    t->active    = TRUE;
    t->startedAt = now;
}

static void stopTimer(TimedMode *t) { t->active = FALSE; }

static void startAccessoryTimer(unsigned long now)
{
    accessoryTimer.active    = TRUE;
    accessoryTimer.startedAt = now;
    accessoryDoorCyclePending = FALSE;
}

static void stopAccessoryTimer(void)
{
    if (!accessoryTimer.active) return;
    accessoryTimer.active     = FALSE;
    accessoryDoorCyclePending = FALSE;
}

static void cancelAllTimedModes(void)
{
    stopTimer(&leavingHomeTimer);
    stopTimer(&courtesyDoorTimer);
    stopTimer(&followMeHomeTimer);
    stopAccessoryTimer();
}

static void expireTimers(unsigned long now)
{
    if (leavingHomeTimer.active  && remainingTimerMs(&leavingHomeTimer,  now) == 0) stopTimer(&leavingHomeTimer);
    if (courtesyDoorTimer.active && remainingTimerMs(&courtesyDoorTimer, now) == 0) stopTimer(&courtesyDoorTimer);
    if (followMeHomeTimer.active && remainingTimerMs(&followMeHomeTimer, now) == 0) stopTimer(&followMeHomeTimer);
    if (accessoryTimer.active    && remainingTimerMs(&accessoryTimer,    now) == 0) stopAccessoryTimer();
}

static void updateCourtesyDoorArm(unsigned long now)
{
    if (doorOpenEvent) {
        courtesyDoorArmed = courtesyDoorReararmPending = FALSE;
        return;
    }
    if (doorCloseEvent) {
        courtesyDoorReararmPending = TRUE;
        courtesyDoorClosedAt       = now;
        return;
    }
    if (courtesyDoorReararmPending && isDoorClosed() &&
        (now - courtesyDoorClosedAt) >= CFG_COURTESY_DOOR_REARM_MS) {
        courtesyDoorReararmPending = FALSE;
        courtesyDoorArmed          = TRUE;
    }
}

static void updateLdrFaultState(unsigned long now)
{
    bool_t faultCandidate = ambientLdrRaw <= CFG_LDR_FAULT_LOW_RAW ||
                            ambientLdrRaw >= CFG_LDR_FAULT_HIGH_RAW;

    if (faultCandidate != ldrFaultPending) {
        ldrFaultPending   = faultCandidate;
        ldrFaultChangedAt = now;
        return;
    }
    if (faultCandidate == ldrFaultActive || (now - ldrFaultChangedAt) < CFG_LDR_FAULT_STABLE_MS) return;

    ldrFaultActive = faultCandidate;
    if (!ldrFaultActive) ambientChangedAt = now;
}

static void initAmbientState(unsigned long now)
{
    ambientLdrRaw      = analogRead(IN_LDR);
    ambientDark        = ambientLdrRaw >= ((CFG_LDR_DARK_THRESHOLD + CFG_LDR_BRIGHT_THRESHOLD) / 2);
    ambientPendingDark = ambientDark;
    ambientChangedAt   = now;
    ambientInitialized = TRUE;
    ldrFaultPending    = ambientLdrRaw <= CFG_LDR_FAULT_LOW_RAW || ambientLdrRaw >= CFG_LDR_FAULT_HIGH_RAW;
    ldrFaultChangedAt  = now;
}

static void updateAmbientState(unsigned long now)
{
    bool_t targetDark;
    ambientLdrRaw = analogRead(IN_LDR);

    if (!ambientInitialized) {
        initAmbientState(now);
        return;
    }

    updateLdrFaultState(now);
    if (ldrFaultActive) return;

    targetDark = ambientDark;
    if (!ambientDark && ambientLdrRaw >= CFG_LDR_DARK_THRESHOLD)   targetDark = TRUE;
    else if (ambientDark && ambientLdrRaw <= CFG_LDR_BRIGHT_THRESHOLD) targetDark = FALSE;

    if (targetDark == ambientDark)      { ambientPendingDark = ambientDark; return; }
    if (targetDark != ambientPendingDark) { ambientPendingDark = targetDark; ambientChangedAt = now; return; }
    if ((now - ambientChangedAt) < CFG_AMBIENT_STABLE_MS) return;

    ambientDark = ambientPendingDark = targetDark;
}

static void readInputsAndEvents(unsigned long now)
{
    lockPulseEvent = unlockPulseEvent = turnPulseEvent = FALSE;
    doorOpenEvent  = doorCloseEvent   = FALSE;
    ignitionOnEvent = ignitionOffEvent = FALSE;

    if (updateInput(&inLock,   now) && isPulseActive(&inLock))   lockPulseEvent   = TRUE;
    if (updateInput(&inUnlock, now) && isPulseActive(&inUnlock)) unlockPulseEvent = TRUE;
    if (updateInput(&inTurn,   now) && isPulseActive(&inTurn))   turnPulseEvent   = TRUE;

    if (updateInput(&inDoor, now)) {
        if (isDoorOpen()) doorOpenEvent = TRUE; else doorCloseEvent = TRUE;
    }
    if (updateInput(&inIgnition, now)) {
        if (isIgnitionOn()) ignitionOnEvent = TRUE; else ignitionOffEvent = TRUE;
    }
}

static void processLogic(unsigned long now)
{
    bool_t ignitionOn        = isIgnitionOn();
    bool_t ignitionOff       = !ignitionOn;
    bool_t ambientUsableDark = ambientDark && !ldrFaultActive;
    bool_t leavingHomeEligible = ignitionOff && isDoorClosed() && ambientUsableDark;
    SignalMatchType signalMatch;

    updateCourtesyDoorArm(now);

    if (doorOpenEvent && accessoryTimer.active)
        accessoryDoorCyclePending = TRUE;

    if (doorCloseEvent && accessoryTimer.active && accessoryDoorCyclePending)
        stopAccessoryTimer();

    if (ignitionOnEvent) {
        cancelAllTimedModes();
        clearSignalMatches();
    }

    if (ignitionOffEvent)
        startAccessoryTimer(now);

    if (!leavingHomeEligible) {
        clearSignalMatches();
    } else {
        expireSignalMatchWindow(now);

        if (lockPulseEvent)   { lastLockPulseAt   = now; lockPulsePending   = TRUE; }
        if (unlockPulseEvent) { lastUnlockPulseAt = now; unlockPulsePending = TRUE; }
        if (turnPulseEvent)   { lastTurnPulseAt   = now; turnPulsePending   = TRUE; }

        signalMatch = consumeSignalMatch(now);

        if (signalMatch == SIGNAL_MATCH_LOCK) {
            leavingHomeTimer.duration = CFG_LEAVING_HOME_LOCK_MS;
            startOrExtendTimer(&leavingHomeTimer, now);
        } else if (signalMatch == SIGNAL_MATCH_UNLOCK) {
            leavingHomeTimer.duration = CFG_LEAVING_HOME_UNLOCK_MS;
            startOrExtendTimer(&leavingHomeTimer, now);
        }
    }

    if (ignitionOff && ambientUsableDark && doorOpenEvent && courtesyDoorArmed)
        startOrExtendTimer(&courtesyDoorTimer, now);

    if (ignitionOffEvent && ambientUsableDark && autoDriveActivePrev)
        startOrExtendTimer(&followMeHomeTimer, now);

    expireTimers(now);
    autoDriveActive     = ignitionOn && ambientUsableDark;
    autoDriveActivePrev = autoDriveActive;
}

static void updateOutput(void)
{
    bool_t outputOn    = autoDriveActive || leavingHomeTimer.active ||
                         courtesyDoorTimer.active || followMeHomeTimer.active;
    bool_t accessoryOn = isIgnitionOn() || accessoryTimer.active;

    digitalWrite(OUT_HEADLIGHT, outputOn    ? HIGH : LOW);
    digitalWrite(OUT_ACCESSORY, accessoryOn ? HIGH : LOW);
}

/* ===========================================================
 * Setup e loop (Arduino framework)
 * =========================================================== */
void setup(void)
{
    unsigned long now;

    pinMode(IN_LOCK,        INPUT_PULLUP);
    pinMode(IN_TURN_SIGNAL, INPUT_PULLUP);
    pinMode(IN_DOOR,        INPUT_PULLUP);
    pinMode(IN_IGNITION,    INPUT_PULLUP);
    pinMode(IN_UNLOCK,      INPUT_PULLUP);

    pinMode(OUT_HEADLIGHT, OUTPUT);
    digitalWrite(OUT_HEADLIGHT, LOW);
    pinMode(OUT_ACCESSORY, OUTPUT);
    digitalWrite(OUT_ACCESSORY, LOW);

    now = millis();

    inLock.stable     = normalizeRawInput(digitalRead(IN_LOCK),        inLock.invert);
    inLock.rawPrev    = inLock.stable;    inLock.changedAt    = now;

    inTurn.stable     = normalizeRawInput(digitalRead(IN_TURN_SIGNAL), inTurn.invert);
    inTurn.rawPrev    = inTurn.stable;    inTurn.changedAt    = now;

    inDoor.stable     = normalizeRawInput(digitalRead(IN_DOOR),        inDoor.invert);
    inDoor.rawPrev    = inDoor.stable;    inDoor.changedAt    = now;

    inIgnition.stable  = normalizeRawInput(digitalRead(IN_IGNITION),   inIgnition.invert);
    inIgnition.rawPrev = inIgnition.stable; inIgnition.changedAt = now;

    inUnlock.stable   = normalizeRawInput(digitalRead(IN_UNLOCK),      inUnlock.invert);
    inUnlock.rawPrev  = inUnlock.stable;  inUnlock.changedAt  = now;

    initAmbientState(now);
    courtesyDoorArmed          = isDoorClosed();
    courtesyDoorReararmPending = FALSE;
    courtesyDoorClosedAt       = now;
    autoDriveActive            = isIgnitionOn() && ambientDark && !ldrFaultActive;
    autoDriveActivePrev        = autoDriveActive;
}

void loop(void)
{
    unsigned long now = millis();
    updateAmbientState(now);
    readInputsAndEvents(now);
    processLogic(now);
    updateOutput();
}
