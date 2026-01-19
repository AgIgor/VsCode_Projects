#include "ladder.h"
#include <ArduinoJson.h>
#include <Preferences.h>

extern Preferences prefs;

bool inputs[4];
bool outputs[6];

struct Timer {
  unsigned long start;
  unsigned long pt;
  bool active;
};

Timer timers[6];
DynamicJsonDocument program(2048);

void initIO() {
  pinMode(32, INPUT_PULLUP);
  pinMode(33, INPUT_PULLUP);
  pinMode(25, INPUT_PULLUP);
  pinMode(26, INPUT_PULLUP);

  pinMode(27, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(2, OUTPUT);
}

void readInputs() {
  inputs[0] = !digitalRead(32);
  inputs[1] = !digitalRead(33);
  inputs[2] = !digitalRead(25);
  inputs[3] = !digitalRead(26);
}

bool evalLogic(JsonArray logic) {
  bool stack[8];
  int sp = 0;

  for (JsonObject op : logic) {
    String type = op["type"];
    if (type == "IN") {
      stack[sp++] = inputs[op["id"].as<int>()];
    } else if (type == "NOT") {
      stack[sp-1] = !stack[sp-1];
    } else if (type == "AND") {
      stack[sp-2] &= stack[sp-1];
      sp--;
    } else if (type == "OR") {
      stack[sp-2] |= stack[sp-1];
      sp--;
    }
  }
  return stack[0];
}

bool evalTON(int idx, bool in, unsigned long pt) {
  if (in) {
    if (!timers[idx].active) {
      timers[idx].start = millis();
      timers[idx].active = true;
    }
    if (millis() - timers[idx].start >= pt) return true;
  } else {
    timers[idx].active = false;
  }
  return false;
}

void scanCycle() {
  readInputs();

  for (JsonObject rung : program["rungs"].as<JsonArray>()) {
    bool res = evalLogic(rung["logic"].as<JsonArray>());

    if (rung.containsKey("timer")) {
      res = evalTON(0, res, rung["timer"]["pt"]);
    }

    outputs[rung["output"].as<int>()] = res;
  }

  digitalWrite(27, outputs[0]);
}

void saveProgram(String json) {
  prefs.putString("prog", json);
}

void loadProgram() {
  deserializeJson(program, prefs.getString("prog", "{}"));
}
