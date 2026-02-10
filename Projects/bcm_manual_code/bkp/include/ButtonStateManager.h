#ifndef BUTTON_STATE_MANAGER_H
#define BUTTON_STATE_MANAGER_H

#include <Arduino.h>

// ==================== ENUM DEFINITIONS ====================
enum EdgeType { EDGE_RISING = 1, EDGE_FALLING = 0 };
enum LogicMode { GATE_AND = 0, GATE_OR = 1 };

// ==================== BUTTON DEBOUNCER ====================
class Button {
private:
  uint8_t pin;
  uint32_t debounceDelay;
  uint32_t lastDebounceTime;
  uint8_t lastStableState;
  uint8_t currentState;
  EdgeType edgeType;  // Rising or Falling edge

public:
  Button(uint8_t buttonPin, uint8_t initialState = HIGH, uint32_t debounceMs = 20, EdgeType edge = EDGE_FALLING);
  
  void update();
  bool edgeDetected();  // Returns true if edge detected
  uint8_t getState();
  void setEdgeType(EdgeType edge);
};

// ==================== OUTPUT TIMER ====================
class OutputTimer {
private:
  uint8_t outputPin;
  uint32_t delayOnTime;
  uint32_t delayOffTime;
  uint32_t startTime;
  bool isActive;
  bool isDelayingOn;
  bool isDelayingOff;
  bool delayOffOnRelease;  // Inicia contagem de delay off apenas ao soltar
  uint8_t currentState;

public:
  OutputTimer(uint8_t pin, uint32_t delayOn = 0, uint32_t delayOff = 0, bool delayOffOnRls = false);
  
  void setDelays(uint32_t delayOn, uint32_t delayOff);
  void setDelayOffOnRelease(bool value);
  void trigger(bool buttonPressed);
  void update();
  uint8_t getOutputState();
  void setOutputPin(uint8_t pin);
  bool isTimingOn();
  bool isTimingOff();
};

// ==================== LOGIC GATE SYSTEM ====================
class LogicGate {
private:
  uint8_t inputCount;
  bool* inputStates;
  LogicMode mode;

public:
  LogicGate(uint8_t numInputs, LogicMode logicMode = GATE_AND);
  ~LogicGate();
  
  void setInput(uint8_t inputIndex, bool state);
  bool evaluate();
  void setMode(LogicMode mode);
  LogicMode getMode();
};

// ==================== PRIORITY OUTPUT ====================
class PriorityOutput {
private:
  uint8_t outputPin;
  uint8_t primaryInput;
  uint8_t secondaryInput;
  uint8_t priorityLevel;  // 1 = primary tem prioridade, 0 = secondary
  uint32_t delayOnTime;
  uint32_t delayOffTime;
  uint32_t startTime;
  bool isActive;
  uint8_t currentState;

public:
  PriorityOutput(uint8_t pin, uint8_t primary, uint8_t secondary, uint8_t priority = 1);
  
  void setInputs(uint8_t primary, uint8_t secondary, uint8_t priority);
  void setDelays(uint32_t delayOn, uint32_t delayOff);
  void update(bool primaryState, bool secondaryState);
  uint8_t getOutputState();
};

// ==================== SCALABLE TIMER ====================
class ScalableTimer {
private:
  uint32_t timerDuration;
  uint32_t startTime;
  bool isRunning;
  bool isCompleted;
  void (*onComplete)() = nullptr;  // Callback function

public:
  ScalableTimer(uint32_t durationMs = 0);
  
  void start(uint32_t durationMs = 0);
  void stop();
  void reset();
  void update();
  
  bool isActive();
  bool hasCompleted();
  uint32_t getRemainingTime();
  uint8_t getProgress();  // 0-100%
  
  void onTimerComplete(void (*callback)());
};

#endif
