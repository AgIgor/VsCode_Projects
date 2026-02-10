#include "ButtonStateManager.h"

// ==================== BUTTON IMPLEMENTATION ====================
Button::Button(uint8_t buttonPin, uint8_t initialState, uint32_t debounceMs, EdgeType edge)
  : pin(buttonPin), debounceDelay(debounceMs), lastStableState(initialState), 
    currentState(initialState), edgeType(edge), lastDebounceTime(0) {
  pinMode(pin, INPUT_PULLUP);
}

void Button::update() {
  uint8_t reading = digitalRead(pin);
  
  if (reading != currentState) {
    lastDebounceTime = millis();
    currentState = reading;
    return;
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (lastStableState != currentState) {
      lastStableState = currentState;
    }
  }
}

bool Button::edgeDetected() {
  uint8_t reading = digitalRead(pin);
  
  if (edgeType == EDGE_RISING) {
    return (lastStableState == LOW && reading == HIGH);
  } else {
    return (lastStableState == HIGH && reading == LOW);
  }
}

uint8_t Button::getState() {
  return lastStableState;
}

void Button::setEdgeType(EdgeType edge) {
  edgeType = edge;
}

// ==================== OUTPUT TIMER IMPLEMENTATION ====================
OutputTimer::OutputTimer(uint8_t pin, uint32_t delayOn, uint32_t delayOff, bool delayOffOnRls)
  : outputPin(pin), delayOnTime(delayOn), delayOffTime(delayOff), 
    delayOffOnRelease(delayOffOnRls), isActive(false), isDelayingOn(false), 
    isDelayingOff(false), currentState(LOW), startTime(0) {
  pinMode(outputPin, OUTPUT);
  digitalWrite(outputPin, currentState);
}

void OutputTimer::setDelays(uint32_t delayOn, uint32_t delayOff) {
  delayOnTime = delayOn;
  delayOffTime = delayOff;
}

void OutputTimer::setDelayOffOnRelease(bool value) {
  delayOffOnRelease = value;
}

void OutputTimer::trigger(bool buttonPressed) {
  if (buttonPressed && !isActive) {
    // Botão pressionado - inicia delay ON
    isActive = true;
    isDelayingOn = true;
    isDelayingOff = false;
    startTime = millis();
  } else if (!buttonPressed && isActive) {
    // Botão solto
    if (delayOffOnRelease) {
      // Inicia delay off apenas ao soltar
      isDelayingOn = false;
      isDelayingOff = true;
      startTime = millis();
    } else {
      // Desativa imediatamente ou com delay off normal
      if (delayOffTime == 0) {
        isActive = false;
        currentState = LOW;
        digitalWrite(outputPin, currentState);
      } else {
        isDelayingOff = true;
        isDelayingOn = false;
        startTime = millis();
      }
    }
  }
}

void OutputTimer::update() {
  if (!isActive && !isDelayingOff) return;
  
  uint32_t elapsed = millis() - startTime;
  
  if (isDelayingOn) {
    if (delayOnTime == 0) {
      // Liga instantâneamente
      currentState = HIGH;
      digitalWrite(outputPin, currentState);
      isDelayingOn = false;
      isDelayingOff = true;
      startTime = millis();
    } else if (elapsed >= delayOnTime) {
      // Tempo de ligação expirou
      currentState = HIGH;
      digitalWrite(outputPin, currentState);
      isDelayingOn = false;
      if (delayOffTime > 0) {
        isDelayingOff = true;
        startTime = millis();
      }
    }
  } else if (isDelayingOff) {
    if (delayOffTime == 0) {
      // Desliga apenas ao soltar o botão (espera comando)
      // Neste caso, o desligamento ocorre quando trigger recebe false
      return;
    } else if (elapsed >= delayOffTime) {
      // Tempo de desligação expirou
      currentState = LOW;
      digitalWrite(outputPin, currentState);
      isDelayingOff = false;
      isActive = false;
    }
  }
}

uint8_t OutputTimer::getOutputState() {
  return currentState;
}

void OutputTimer::setOutputPin(uint8_t pin) {
  outputPin = pin;
  pinMode(outputPin, OUTPUT);
  digitalWrite(outputPin, currentState);
}

bool OutputTimer::isTimingOn() {
  return isDelayingOn;
}

bool OutputTimer::isTimingOff() {
  return isDelayingOff;
}

// ==================== LOGIC GATE IMPLEMENTATION ====================
LogicGate::LogicGate(uint8_t numInputs, LogicMode logicMode)
  : inputCount(numInputs), mode(logicMode) {
  inputStates = new bool[numInputs];
  for (uint8_t i = 0; i < numInputs; i++) {
    inputStates[i] = false;
  }
}

LogicGate::~LogicGate() {
  delete[] inputStates;
}

void LogicGate::setInput(uint8_t inputIndex, bool state) {
  if (inputIndex < inputCount) {
    inputStates[inputIndex] = state;
  }
}

bool LogicGate::evaluate() {
  if (mode == GATE_AND) {
    // Todos os inputs devem ser verdadeiros
    for (uint8_t i = 0; i < inputCount; i++) {
      if (!inputStates[i]) return false;
    }
    return true;
  } else {
    // Pelo menos um input deve ser verdadeiro
    for (uint8_t i = 0; i < inputCount; i++) {
      if (inputStates[i]) return true;
    }
    return false;
  }
}

void LogicGate::setMode(LogicMode mode) {
  this->mode = mode;
}

LogicMode LogicGate::getMode() {
  return mode;
}

// ==================== PRIORITY OUTPUT IMPLEMENTATION ====================
PriorityOutput::PriorityOutput(uint8_t pin, uint8_t primary, uint8_t secondary, uint8_t priority)
  : outputPin(pin), primaryInput(primary), secondaryInput(secondary), 
    priorityLevel(priority), delayOnTime(0), delayOffTime(0), 
    isActive(false), currentState(LOW), startTime(0) {
  pinMode(outputPin, OUTPUT);
  digitalWrite(outputPin, currentState);
}

void PriorityOutput::setInputs(uint8_t primary, uint8_t secondary, uint8_t priority) {
  primaryInput = primary;
  secondaryInput = secondary;
  priorityLevel = priority;
}

void PriorityOutput::setDelays(uint32_t delayOn, uint32_t delayOff) {
  delayOnTime = delayOn;
  delayOffTime = delayOff;
}

void PriorityOutput::update(bool primaryState, bool secondaryState) {
  bool shouldActivate = false;
  
  if (priorityLevel == 1) {
    // Primary tem prioridade
    shouldActivate = primaryState || secondaryState;
    if (primaryState) shouldActivate = true;
    else shouldActivate = secondaryState;
  } else {
    // Secondary tem prioridade
    shouldActivate = primaryState || secondaryState;
    if (secondaryState) shouldActivate = true;
    else shouldActivate = primaryState;
  }
  
  if (shouldActivate && !isActive) {
    isActive = true;
    startTime = millis();
  } else if (!shouldActivate && isActive) {
    uint32_t elapsed = millis() - startTime;
    if (delayOffTime == 0 || elapsed >= delayOffTime) {
      isActive = false;
      currentState = LOW;
      digitalWrite(outputPin, currentState);
    }
  }
  
  if (isActive) {
    uint32_t elapsed = millis() - startTime;
    if (currentState == LOW && elapsed >= delayOnTime) {
      currentState = HIGH;
      digitalWrite(outputPin, currentState);
    }
  }
}

uint8_t PriorityOutput::getOutputState() {
  return currentState;
}

// ==================== SCALABLE TIMER IMPLEMENTATION ====================
ScalableTimer::ScalableTimer(uint32_t durationMs)
  : timerDuration(durationMs), isRunning(false), isCompleted(false), startTime(0) {}

void ScalableTimer::start(uint32_t durationMs) {
  if (durationMs > 0) {
    timerDuration = durationMs;
  }
  isRunning = true;
  isCompleted = false;
  startTime = millis();
}

void ScalableTimer::stop() {
  isRunning = false;
  isCompleted = false;
}

void ScalableTimer::reset() {
  isRunning = false;
  isCompleted = false;
  startTime = millis();
}

void ScalableTimer::update() {
  if (!isRunning) return;
  
  uint32_t elapsed = millis() - startTime;
  
  if (elapsed >= timerDuration) {
    isRunning = false;
    isCompleted = true;
    if (onComplete != nullptr) {
      onComplete();
    }
  }
}

bool ScalableTimer::isActive() {
  return isRunning;
}

bool ScalableTimer::hasCompleted() {
  return isCompleted;
}

uint32_t ScalableTimer::getRemainingTime() {
  if (!isRunning) return 0;
  uint32_t elapsed = millis() - startTime;
  if (elapsed >= timerDuration) return 0;
  return timerDuration - elapsed;
}

uint8_t ScalableTimer::getProgress() {
  if (!isRunning) return 0;
  uint32_t elapsed = millis() - startTime;
  if (elapsed >= timerDuration) return 100;
  return (elapsed * 100) / timerDuration;
}

void ScalableTimer::onTimerComplete(void (*callback)()) {
  onComplete = callback;
}
