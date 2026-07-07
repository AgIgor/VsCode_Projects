#include "script_vm.h"

ScriptVM::ScriptVM(CounterStore& counters)
    : counters_(counters),
      instructionCount_(0),
      pc_(0),
      running_(false),
      waitUntilMs_(0),
      lastError_("") {}

bool ScriptVM::loadScript(const String& script, String& error) {
  stop();
  instructionCount_ = 0;

  size_t lineNumber = 1;
  int start = 0;

  while (start <= script.length()) {
    int end = script.indexOf('\n', start);
    if (end < 0) {
      end = script.length();
    }

    String line = script.substring(start, end);
    line.replace("\r", "");
    line.trim();

    if (!line.isEmpty()) {
      Instruction instruction{};
      if (!parseLine(line, lineNumber, instruction, error)) {
        lastError_ = error;
        return false;
      }

      if (instructionCount_ >= kMaxInstructions) {
        error = "Script excede o limite de instrucoes (128).";
        lastError_ = error;
        return false;
      }

      instructions_[instructionCount_++] = instruction;
    }

    if (end >= script.length()) {
      break;
    }

    start = end + 1;
    ++lineNumber;
  }

  lastError_ = "";
  return true;
}

void ScriptVM::start() {
  pc_ = 0;
  waitUntilMs_ = 0;
  running_ = instructionCount_ > 0;
}

void ScriptVM::stop() {
  running_ = false;
  waitUntilMs_ = 0;
}

void ScriptVM::tick() {
  if (!running_) {
    return;
  }

  if (isWaiting()) {
    return;
  }

  while (running_ && pc_ < instructionCount_) {
    const Instruction& instruction = instructions_[pc_++];
    execute(instruction);

    if (instruction.type == OpType::Wait) {
      break;
    }
  }

  if (pc_ >= instructionCount_) {
    running_ = false;
  }
}

bool ScriptVM::isRunning() const {
  return running_;
}

String ScriptVM::getLastError() const {
  return lastError_;
}

String ScriptVM::getStatusJson() const {
  String json = "{";
  json += "\"running\":" + String(running_ ? "true" : "false");
  json += ",\"pc\":" + String(static_cast<uint32_t>(pc_));
  json += ",\"instructionCount\":" + String(static_cast<uint32_t>(instructionCount_));
  json += ",\"lastError\":\"";

  String safeError = lastError_;
  safeError.replace("\\", "\\\\");
  safeError.replace("\"", "\\\"");

  json += safeError + "\"";
  json += ",\"counters\":" + counters_.toJson();
  json += "}";
  return json;
}

bool ScriptVM::parseLine(const String& line, size_t lineNumber, Instruction& out, String& error) {
  String raw = line;
  const int commentPos = raw.indexOf('#');
  if (commentPos >= 0) {
    raw = raw.substring(0, commentPos);
  }

  raw.trim();
  if (raw.isEmpty()) {
    return true;
  }

  size_t pos = 0;
  String cmd = nextToken(raw, pos);
  cmd.toUpperCase();

  if (cmd == "ON" || cmd == "OFF") {
    const String pinToken = nextToken(raw, pos);
    int32_t pin = 0;
    if (pinToken.isEmpty() || !parseInt32(pinToken, pin) || pin < 0) {
      error = "Linha " + String(lineNumber) + ": pino invalido para comando " + cmd + ".";
      return false;
    }

    out.type = (cmd == "ON") ? OpType::On : OpType::Off;
    out.pin = static_cast<int>(pin);
    out.waitMs = 0;
    out.counter = "";
    out.value = 0;
    return true;
  }

  if (cmd == "WAIT" || cmd == "DELAY") {
    const String delayToken = nextToken(raw, pos);
    uint32_t delayMs = 0;
    if (delayToken.isEmpty() || !parseUint32(delayToken, delayMs)) {
      error = "Linha " + String(lineNumber) + ": delay invalido.";
      return false;
    }

    out.type = OpType::Wait;
    out.pin = -1;
    out.waitMs = delayMs;
    out.counter = "";
    out.value = 0;
    return true;
  }

  if (cmd == "COUNT" || cmd == "COUNTER") {
    const String counterName = nextToken(raw, pos);
    String action = nextToken(raw, pos);
    action.toUpperCase();

    if (counterName.isEmpty() || action.isEmpty()) {
      error = "Linha " + String(lineNumber) + ": sintaxe de contador invalida.";
      return false;
    }

    out.pin = -1;
    out.waitMs = 0;
    out.counter = counterName;
    out.value = 0;

    if (action == "INC") {
      String amountToken = nextToken(raw, pos);
      if (amountToken.isEmpty()) {
        out.value = 1;
      } else if (!parseInt32(amountToken, out.value)) {
        error = "Linha " + String(lineNumber) + ": valor invalido em COUNT INC.";
        return false;
      }

      out.type = OpType::CounterInc;
      return true;
    }

    if (action == "DEC") {
      String amountToken = nextToken(raw, pos);
      if (amountToken.isEmpty()) {
        out.value = 1;
      } else if (!parseInt32(amountToken, out.value)) {
        error = "Linha " + String(lineNumber) + ": valor invalido em COUNT DEC.";
        return false;
      }

      out.type = OpType::CounterDec;
      return true;
    }

    if (action == "SET") {
      String valueToken = nextToken(raw, pos);
      if (valueToken.isEmpty() || !parseInt32(valueToken, out.value)) {
        error = "Linha " + String(lineNumber) + ": valor invalido em COUNT SET.";
        return false;
      }

      out.type = OpType::CounterSet;
      return true;
    }

    if (action == "RESET") {
      out.type = OpType::CounterReset;
      return true;
    }

    error = "Linha " + String(lineNumber) + ": acao de contador desconhecida (" + action + ").";
    return false;
  }

  error = "Linha " + String(lineNumber) + ": comando desconhecido (" + cmd + ").";
  return false;
}

String ScriptVM::nextToken(const String& line, size_t& pos) {
  while (pos < static_cast<size_t>(line.length()) && line[static_cast<unsigned int>(pos)] == ' ') {
    ++pos;
  }

  if (pos >= static_cast<size_t>(line.length())) {
    return "";
  }

  size_t start = pos;
  while (pos < static_cast<size_t>(line.length()) && line[static_cast<unsigned int>(pos)] != ' ') {
    ++pos;
  }

  return line.substring(start, pos);
}

bool ScriptVM::parseInt32(const String& token, int32_t& out) {
  if (token.isEmpty()) {
    return false;
  }

  char* end = nullptr;
  const long parsed = strtol(token.c_str(), &end, 10);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }

  out = static_cast<int32_t>(parsed);
  return true;
}

bool ScriptVM::parseUint32(const String& token, uint32_t& out) {
  if (token.isEmpty()) {
    return false;
  }

  char* end = nullptr;
  const unsigned long parsed = strtoul(token.c_str(), &end, 10);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }

  out = static_cast<uint32_t>(parsed);
  return true;
}

bool ScriptVM::isWaiting() const {
  if (waitUntilMs_ == 0) {
    return false;
  }

  return static_cast<int32_t>(millis() - waitUntilMs_) < 0;
}

void ScriptVM::execute(const Instruction& instruction) {
  switch (instruction.type) {
    case OpType::On:
      pinMode(instruction.pin, OUTPUT);
      digitalWrite(instruction.pin, HIGH);
      break;

    case OpType::Off:
      pinMode(instruction.pin, OUTPUT);
      digitalWrite(instruction.pin, LOW);
      break;

    case OpType::Wait:
      waitUntilMs_ = millis() + instruction.waitMs;
      break;

    case OpType::CounterInc:
      counters_.increment(instruction.counter, instruction.value);
      break;

    case OpType::CounterDec:
      counters_.decrement(instruction.counter, instruction.value);
      break;

    case OpType::CounterSet:
      counters_.set(instruction.counter, instruction.value);
      break;

    case OpType::CounterReset:
      counters_.reset(instruction.counter);
      break;
  }
}
