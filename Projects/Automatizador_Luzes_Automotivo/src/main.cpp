#include <Arduino.h>
#include "ConfigManager.h"
#include "WebConfig.h"
#include "PinConfig.h"

// Global objects (lazy initialization)
ConfigManager* configManager = nullptr;
WebServerManager* webServer = nullptr;

// ===== System State Variables =====
struct SystemState {
  // Input states
  uint8_t lockState = HIGH;
  uint8_t turnSignalState = HIGH;
  uint8_t doorState = HIGH;
  uint8_t ignitionState = HIGH;
  uint8_t unlockState = HIGH;
  int ldrValue = 0;

  // Output states
  uint8_t headlightState = LOW;
  uint8_t accessoryState = LOW;

  // Timing state
  unsigned long lastInputUpdateMs = 0;
  unsigned long lastStatusUpdateMs = 0;
};

SystemState systemState;

// ===== GPIO Initialization =====
void initializeGPIO() {
  // GPIO already initialized in setup(), skipping
  Serial.println("GPIO já foi inicializado no setup()");
}

// ===== Input Reading =====
void updateInputs() {
  // Simplified - skip ADC to avoid issues
  if (!configManager) return;
  
  SystemConfig& cfg = configManager->getConfig();
  unsigned long now = millis();

  // Read digital inputs with inversion logic
  if (cfg.pins.lock >= 0 && cfg.pins.lock <= 39) {
    systemState.lockState = digitalRead(cfg.pins.lock);
    if (cfg.invertLockInput) systemState.lockState = !systemState.lockState;
  }
  
  if (cfg.pins.door >= 0 && cfg.pins.door <= 39) {
    systemState.doorState = digitalRead(cfg.pins.door);
    if (cfg.invertDoorInput) systemState.doorState = !systemState.doorState;
  }

  // Skip ADC reading for now to avoid issues
  systemState.ldrValue = 0;
  systemState.lastInputUpdateMs = now;
}

// ===== Control Logic =====
void updateControlLogic() {
  if (!configManager) return;
  
  // Simplified - no logic for now to avoid issues
  // Just keep lights off
  systemState.headlightState = LOW;
  systemState.accessoryState = LOW;
}

// ===== Serial Debug Output =====
void printStatus() {
  unsigned long now = millis();

  if (now - systemState.lastStatusUpdateMs < 10000) {
    return; // Print status every 10 seconds
  }

  systemState.lastStatusUpdateMs = now;

  Serial.println("\n=== Status ===");
  Serial.print("Uptime: ");
  Serial.print(now / 1000);
  Serial.println("s");
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.println("==============");
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(2000);  // Aguardar monitor serial

  Serial.println("\n\n=== Automatizador de Luzes Automotivo (ESP32) ===");
  Serial.println("Iniciando...\n");
  Serial.flush();

  // Step 1: Initialize GPIO (sem ADC por enquanto)
  Serial.println("[1/4] Inicializando GPIO...");
  Serial.flush();
  delay(500);
  
  try {
    SystemConfig defaultConfig;  // Use padrão local
    
    // Setup inputs com pull-ups - apenas pinos seguros
    if (defaultConfig.pins.lock >= 0 && defaultConfig.pins.lock <= 39) {
      pinMode(defaultConfig.pins.lock, INPUT_PULLUP);
      Serial.print("  - Lock (GPIO");
      Serial.print(defaultConfig.pins.lock);
      Serial.println(") OK");
    }
    
    if (defaultConfig.pins.door >= 0 && defaultConfig.pins.door <= 39) {
      pinMode(defaultConfig.pins.door, INPUT_PULLUP);
      Serial.print("  - Door (GPIO");
      Serial.print(defaultConfig.pins.door);
      Serial.println(") OK");
    }
    
    // Setup outputs - apenas pinos seguros
    if (defaultConfig.pins.headlight >= 0 && defaultConfig.pins.headlight <= 39) {
      pinMode(defaultConfig.pins.headlight, OUTPUT);
      digitalWrite(defaultConfig.pins.headlight, LOW);
      Serial.print("  - Headlight (GPIO");
      Serial.print(defaultConfig.pins.headlight);
      Serial.println(") OK");
    }
    
    Serial.println("GPIO inicializado com sucesso!");
  } catch (...) {
    Serial.println("ERRO ao inicializar GPIO!");
  }
  Serial.flush();
  delay(500);

  // Step 2: Initialize configuration manager
  Serial.println("[2/4] Inicializando gerenciador de configuração...");
  Serial.flush();
  delay(500);
  
  configManager = new ConfigManager();
  if (!configManager) {
    Serial.println("ERRO: Falha ao inicializar ConfigManager!");
    Serial.flush();
    while(1) delay(1000);
  }
  Serial.println("ConfigManager OK!");
  Serial.flush();
  delay(500);

  // Step 3: Initialize web server
  Serial.println("[3/4] Inicializando servidor web...");
  Serial.flush();
  delay(500);
  
  webServer = new WebServerManager(*configManager);
  if (!webServer) {
    Serial.println("ERRO: Falha ao inicializar WebServerManager!");
    Serial.flush();
    while(1) delay(1000);
  }
  Serial.println("WebServerManager OK!");
  Serial.flush();
  delay(500);

  // Step 4: Start web server
  Serial.println("[4/4] Iniciando servidor web...");
  Serial.flush();
  delay(500);
  
  webServer->begin();
  Serial.println("Servidor web iniciado!");
  Serial.flush();

  Serial.println("\n✓ Sistema pronto!");
  Serial.print("Acesse: http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/");
  Serial.println("\nEntendo em loop principal...");
  Serial.flush();
}

// ===== Main Loop =====
void loop() {
  // Safety check
  if (!configManager || !webServer) {
    Serial.println("Aguardando inicialização...");
    delay(1000);
    return;
  }

  try {
    // Handle web server (prioritário)
    webServer->handle();

    // Small delay to prevent watchdog timeout
    delay(50);
  } 
  catch (...) {
    Serial.println("ERRO no loop principal!");
    Serial.flush();
    delay(1000);
  }
}
