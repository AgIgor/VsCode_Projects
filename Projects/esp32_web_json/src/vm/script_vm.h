#pragma once

#include <Arduino.h>

#include "counters/counter_store.h"

class ScriptVM {
public:
  explicit ScriptVM(CounterStore& counters);

  bool loadScript(const String& script, String& error);
  void start();
  void stop();
  void tick();

  bool isRunning() const;
  String getLastError() const;
  String getStatusJson() const;

private:
  enum class OpType {
    On,
    Off,
    Wait,
    CounterInc,
    CounterDec,
    CounterSet,
    CounterReset
  };

  struct Instruction {
    OpType type;
    int pin;
    uint32_t waitMs;
    String counter;
    int32_t value;
  };

  static constexpr size_t kMaxInstructions = 128;

  CounterStore& counters_;
  Instruction instructions_[kMaxInstructions];
  size_t instructionCount_;
  size_t pc_;
  bool running_;
  uint32_t waitUntilMs_;
  String lastError_;

  bool parseLine(const String& line, size_t lineNumber, Instruction& out, String& error);
  static String nextToken(const String& line, size_t& pos);
  static bool parseInt32(const String& token, int32_t& out);
  static bool parseUint32(const String& token, uint32_t& out);
  bool isWaiting() const;
  void execute(const Instruction& instruction);
};
