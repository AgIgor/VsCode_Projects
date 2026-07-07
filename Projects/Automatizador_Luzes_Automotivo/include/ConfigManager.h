#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "PinConfig.h"

// ===== System Configuration Structure =====
struct SystemConfig {
  // Timing configurations (in milliseconds)
  unsigned long debounceMs = 30;
  unsigned long signalPairWindowMs = 1500;
  unsigned long leavingHomeLockMs = 40000;
  unsigned long leavingHomeUnlockMs = 20000;
  unsigned long courtesyDoorMs = 20000;
  unsigned long followMeHomeMs = 30000;
  unsigned long courtesyDoorReararmMs = 5000;
  unsigned long ambientStableMs = 2500;
  unsigned long ldrFaultStableMs = 4000;
  unsigned long summaryIntervalMs = 5000;
  unsigned long accessoryOnMs = 600000;

  // LDR (Light Sensor) thresholds
  int ldrDarkThreshold = 790;
  int ldrBrightThreshold = 750;
  int ldrFaultLowRaw = 5;
  int ldrFaultHighRaw = 1018;

  // Input inversion flags
  bool invertLockInput = true;
  bool invertTurnSignalInput = true;
  bool invertDoorInput = false;
  bool invertIgnitionInput = true;
  bool invertUnlockInput = true;

  // IO Pin mapping
  struct {
    uint8_t lock = IN_LOCK;
    uint8_t turnSignal = IN_TURN_SIGNAL;
    uint8_t door = IN_DOOR;
    uint8_t ignition = IN_IGNITION;
    uint8_t unlock = IN_UNLOCK;
    uint8_t ldr = IN_LDR;
    uint8_t headlight = OUT_HEADLIGHT;
    uint8_t accessory = OUT_ACCESSORY;
  } pins;

  // Feature flags
  bool enableAutoDrive = true;
  bool enableLeavingHome = true;
  bool enableCourtesyDoor = true;
  bool enableFollowMeHome = true;
};

// ===== Configuration Manager =====
class ConfigManager {
private:
  SystemConfig config;
  bool fsReady = false;

  void initLittleFS() {
    Serial.println("Tentando inicializar LittleFS...");
    if (!LittleFS.begin(true, "/fs", 10)) {
      Serial.println("Aviso: LittleFS falhou, usando padrões em memória");
      fsReady = false;
      return;
    }
    Serial.println("LittleFS inicializado com sucesso");
    fsReady = true;
  }

public:
  ConfigManager() {
    delay(100); // Small delay to let system settle
    initLittleFS();
    if (!loadFromFile()) {
      Serial.println("Usando configuração padrão");
    }
  }

  // Load configuration from file
  bool loadFromFile() {
    if (!fsReady) {
      Serial.println("LittleFS não está disponível, usando padrões");
      return true;
    }

    if (!LittleFS.exists(CONFIG_FILE)) {
      Serial.println("Arquivo de config não existe, usando padrões");
      return true;
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
      Serial.println("Erro ao abrir arquivo de config, usando padrões");
      return true;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
      Serial.print("Erro ao desserializar JSON: ");
      Serial.println(error.c_str());
      return false;
    }

    // Load timing configs
    config.debounceMs = doc["debounceMs"] | config.debounceMs;
    config.signalPairWindowMs = doc["signalPairWindowMs"] | config.signalPairWindowMs;
    config.leavingHomeLockMs = doc["leavingHomeLockMs"] | config.leavingHomeLockMs;
    config.leavingHomeUnlockMs = doc["leavingHomeUnlockMs"] | config.leavingHomeUnlockMs;
    config.courtesyDoorMs = doc["courtesyDoorMs"] | config.courtesyDoorMs;
    config.followMeHomeMs = doc["followMeHomeMs"] | config.followMeHomeMs;
    config.courtesyDoorReararmMs = doc["courtesyDoorReararmMs"] | config.courtesyDoorReararmMs;
    config.ambientStableMs = doc["ambientStableMs"] | config.ambientStableMs;
    config.ldrFaultStableMs = doc["ldrFaultStableMs"] | config.ldrFaultStableMs;
    config.summaryIntervalMs = doc["summaryIntervalMs"] | config.summaryIntervalMs;
    config.accessoryOnMs = doc["accessoryOnMs"] | config.accessoryOnMs;

    // Load LDR thresholds
    config.ldrDarkThreshold = doc["ldrDarkThreshold"] | config.ldrDarkThreshold;
    config.ldrBrightThreshold = doc["ldrBrightThreshold"] | config.ldrBrightThreshold;
    config.ldrFaultLowRaw = doc["ldrFaultLowRaw"] | config.ldrFaultLowRaw;
    config.ldrFaultHighRaw = doc["ldrFaultHighRaw"] | config.ldrFaultHighRaw;

    // Load input inversion flags
    config.invertLockInput = doc["invertLockInput"] | config.invertLockInput;
    config.invertTurnSignalInput = doc["invertTurnSignalInput"] | config.invertTurnSignalInput;
    config.invertDoorInput = doc["invertDoorInput"] | config.invertDoorInput;
    config.invertIgnitionInput = doc["invertIgnitionInput"] | config.invertIgnitionInput;
    config.invertUnlockInput = doc["invertUnlockInput"] | config.invertUnlockInput;

    // Load pins
    if (doc.containsKey("pins")) {
      JsonObject pins = doc["pins"];
      config.pins.lock = pins["lock"] | config.pins.lock;
      config.pins.turnSignal = pins["turnSignal"] | config.pins.turnSignal;
      config.pins.door = pins["door"] | config.pins.door;
      config.pins.ignition = pins["ignition"] | config.pins.ignition;
      config.pins.unlock = pins["unlock"] | config.pins.unlock;
      config.pins.ldr = pins["ldr"] | config.pins.ldr;
      config.pins.headlight = pins["headlight"] | config.pins.headlight;
      config.pins.accessory = pins["accessory"] | config.pins.accessory;
    }

    // Load feature flags
    config.enableAutoDrive = doc["enableAutoDrive"] | config.enableAutoDrive;
    config.enableLeavingHome = doc["enableLeavingHome"] | config.enableLeavingHome;
    config.enableCourtesyDoor = doc["enableCourtesyDoor"] | config.enableCourtesyDoor;
    config.enableFollowMeHome = doc["enableFollowMeHome"] | config.enableFollowMeHome;

    Serial.println("Configuração carregada com sucesso");
    return true;
  }

  // Save configuration to file
  bool saveToFile() {
    if (!fsReady) {
      Serial.println("Aviso: Não foi possível salvar (LittleFS não disponível)");
      return true; // Return true to not block
    }

    DynamicJsonDocument doc(4096);

    // Save timing configs
    doc["debounceMs"] = config.debounceMs;
    doc["signalPairWindowMs"] = config.signalPairWindowMs;
    doc["leavingHomeLockMs"] = config.leavingHomeLockMs;
    doc["leavingHomeUnlockMs"] = config.leavingHomeUnlockMs;
    doc["courtesyDoorMs"] = config.courtesyDoorMs;
    doc["followMeHomeMs"] = config.followMeHomeMs;
    doc["courtesyDoorReararmMs"] = config.courtesyDoorReararmMs;
    doc["ambientStableMs"] = config.ambientStableMs;
    doc["ldrFaultStableMs"] = config.ldrFaultStableMs;
    doc["summaryIntervalMs"] = config.summaryIntervalMs;
    doc["accessoryOnMs"] = config.accessoryOnMs;

    // Save LDR thresholds
    doc["ldrDarkThreshold"] = config.ldrDarkThreshold;
    doc["ldrBrightThreshold"] = config.ldrBrightThreshold;
    doc["ldrFaultLowRaw"] = config.ldrFaultLowRaw;
    doc["ldrFaultHighRaw"] = config.ldrFaultHighRaw;

    // Save input inversion flags
    doc["invertLockInput"] = config.invertLockInput;
    doc["invertTurnSignalInput"] = config.invertTurnSignalInput;
    doc["invertDoorInput"] = config.invertDoorInput;
    doc["invertIgnitionInput"] = config.invertIgnitionInput;
    doc["invertUnlockInput"] = config.invertUnlockInput;

    // Save pins
    JsonObject pins = doc.createNestedObject("pins");
    pins["lock"] = config.pins.lock;
    pins["turnSignal"] = config.pins.turnSignal;
    pins["door"] = config.pins.door;
    pins["ignition"] = config.pins.ignition;
    pins["unlock"] = config.pins.unlock;
    pins["ldr"] = config.pins.ldr;
    pins["headlight"] = config.pins.headlight;
    pins["accessory"] = config.pins.accessory;

    // Save feature flags
    doc["enableAutoDrive"] = config.enableAutoDrive;
    doc["enableLeavingHome"] = config.enableLeavingHome;
    doc["enableCourtesyDoor"] = config.enableCourtesyDoor;
    doc["enableFollowMeHome"] = config.enableFollowMeHome;

    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) {
      Serial.println("Erro ao criar arquivo de config");
      return false;
    }

    if (serializeJson(doc, file) == 0) {
      Serial.println("Erro ao serializar JSON para arquivo");
      file.close();
      return false;
    }

    file.close();
    Serial.println("Configuração salva com sucesso");
    return true;
  }

  // Get configuration as JSON string
  String getConfigJSON() {
    DynamicJsonDocument doc(4096);
    
    doc["debounceMs"] = config.debounceMs;
    doc["signalPairWindowMs"] = config.signalPairWindowMs;
    doc["leavingHomeLockMs"] = config.leavingHomeLockMs;
    doc["leavingHomeUnlockMs"] = config.leavingHomeUnlockMs;
    doc["courtesyDoorMs"] = config.courtesyDoorMs;
    doc["followMeHomeMs"] = config.followMeHomeMs;
    doc["courtesyDoorReararmMs"] = config.courtesyDoorReararmMs;
    doc["ambientStableMs"] = config.ambientStableMs;
    doc["ldrFaultStableMs"] = config.ldrFaultStableMs;
    doc["summaryIntervalMs"] = config.summaryIntervalMs;
    doc["accessoryOnMs"] = config.accessoryOnMs;
    doc["ldrDarkThreshold"] = config.ldrDarkThreshold;
    doc["ldrBrightThreshold"] = config.ldrBrightThreshold;
    doc["ldrFaultLowRaw"] = config.ldrFaultLowRaw;
    doc["ldrFaultHighRaw"] = config.ldrFaultHighRaw;
    doc["invertLockInput"] = config.invertLockInput;
    doc["invertTurnSignalInput"] = config.invertTurnSignalInput;
    doc["invertDoorInput"] = config.invertDoorInput;
    doc["invertIgnitionInput"] = config.invertIgnitionInput;
    doc["invertUnlockInput"] = config.invertUnlockInput;

    JsonObject pins = doc.createNestedObject("pins");
    pins["lock"] = config.pins.lock;
    pins["turnSignal"] = config.pins.turnSignal;
    pins["door"] = config.pins.door;
    pins["ignition"] = config.pins.ignition;
    pins["unlock"] = config.pins.unlock;
    pins["ldr"] = config.pins.ldr;
    pins["headlight"] = config.pins.headlight;
    pins["accessory"] = config.pins.accessory;

    doc["enableAutoDrive"] = config.enableAutoDrive;
    doc["enableLeavingHome"] = config.enableLeavingHome;
    doc["enableCourtesyDoor"] = config.enableCourtesyDoor;
    doc["enableFollowMeHome"] = config.enableFollowMeHome;

    String output;
    serializeJson(doc, output);
    return output;
  }

  // Update configuration from JSON
  bool updateFromJSON(const String& json) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
      Serial.print("Erro ao desserializar JSON: ");
      Serial.println(error.c_str());
      return false;
    }

    if (doc.containsKey("debounceMs")) config.debounceMs = doc["debounceMs"];
    if (doc.containsKey("signalPairWindowMs")) config.signalPairWindowMs = doc["signalPairWindowMs"];
    if (doc.containsKey("leavingHomeLockMs")) config.leavingHomeLockMs = doc["leavingHomeLockMs"];
    if (doc.containsKey("leavingHomeUnlockMs")) config.leavingHomeUnlockMs = doc["leavingHomeUnlockMs"];
    if (doc.containsKey("courtesyDoorMs")) config.courtesyDoorMs = doc["courtesyDoorMs"];
    if (doc.containsKey("followMeHomeMs")) config.followMeHomeMs = doc["followMeHomeMs"];
    if (doc.containsKey("courtesyDoorReararmMs")) config.courtesyDoorReararmMs = doc["courtesyDoorReararmMs"];
    if (doc.containsKey("ambientStableMs")) config.ambientStableMs = doc["ambientStableMs"];
    if (doc.containsKey("ldrFaultStableMs")) config.ldrFaultStableMs = doc["ldrFaultStableMs"];
    if (doc.containsKey("summaryIntervalMs")) config.summaryIntervalMs = doc["summaryIntervalMs"];
    if (doc.containsKey("accessoryOnMs")) config.accessoryOnMs = doc["accessoryOnMs"];
    
    if (doc.containsKey("ldrDarkThreshold")) config.ldrDarkThreshold = doc["ldrDarkThreshold"];
    if (doc.containsKey("ldrBrightThreshold")) config.ldrBrightThreshold = doc["ldrBrightThreshold"];
    if (doc.containsKey("ldrFaultLowRaw")) config.ldrFaultLowRaw = doc["ldrFaultLowRaw"];
    if (doc.containsKey("ldrFaultHighRaw")) config.ldrFaultHighRaw = doc["ldrFaultHighRaw"];
    
    if (doc.containsKey("invertLockInput")) config.invertLockInput = doc["invertLockInput"];
    if (doc.containsKey("invertTurnSignalInput")) config.invertTurnSignalInput = doc["invertTurnSignalInput"];
    if (doc.containsKey("invertDoorInput")) config.invertDoorInput = doc["invertDoorInput"];
    if (doc.containsKey("invertIgnitionInput")) config.invertIgnitionInput = doc["invertIgnitionInput"];
    if (doc.containsKey("invertUnlockInput")) config.invertUnlockInput = doc["invertUnlockInput"];

    if (doc.containsKey("pins")) {
      JsonObject pins = doc["pins"];
      if (pins.containsKey("lock")) config.pins.lock = pins["lock"];
      if (pins.containsKey("turnSignal")) config.pins.turnSignal = pins["turnSignal"];
      if (pins.containsKey("door")) config.pins.door = pins["door"];
      if (pins.containsKey("ignition")) config.pins.ignition = pins["ignition"];
      if (pins.containsKey("unlock")) config.pins.unlock = pins["unlock"];
      if (pins.containsKey("ldr")) config.pins.ldr = pins["ldr"];
      if (pins.containsKey("headlight")) config.pins.headlight = pins["headlight"];
      if (pins.containsKey("accessory")) config.pins.accessory = pins["accessory"];
    }

    if (doc.containsKey("enableAutoDrive")) config.enableAutoDrive = doc["enableAutoDrive"];
    if (doc.containsKey("enableLeavingHome")) config.enableLeavingHome = doc["enableLeavingHome"];
    if (doc.containsKey("enableCourtesyDoor")) config.enableCourtesyDoor = doc["enableCourtesyDoor"];
    if (doc.containsKey("enableFollowMeHome")) config.enableFollowMeHome = doc["enableFollowMeHome"];

    return saveToFile();
  }

  // Getter methods
  SystemConfig& getConfig() { return config; }
  const SystemConfig& getConfig() const { return config; }
};
