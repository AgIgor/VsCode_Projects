#pragma once

// ===== GPIO Configuration for ESP32 =====
// Você pode modificar estes valores de acordo com sua montagem

// ===== INPUTS (Entradas) =====
const uint8_t IN_LOCK = 4;           // Pino de entrada: trava (normalmente 1, pulso em 0)
const uint8_t IN_TURN_SIGNAL = 5;    // Pino de entrada: seta (normalmente 1, pulso em 0)
const uint8_t IN_DOOR = 6;           // Pino de entrada: porta (0 aberta, 1 fechada)
const uint8_t IN_IGNITION = 7;       // Pino de entrada: ignição (0 ligada, 1 desligada)
const uint8_t IN_UNLOCK = 8;         // Pino de entrada: destrava (normalmente 1, pulso em 0)
const uint8_t IN_LDR = 35;           // Pino ADC: LDR (sensor de luz)

// ===== OUTPUTS (Saídas) =====
const uint8_t OUT_HEADLIGHT = 2;     // Pino de saída: farol
const uint8_t OUT_ACCESSORY = 3;     // Pino de saída: acessório

// ===== EEPROM/SPIFFS Settings =====
const char* CONFIG_FILE = "/config.json";
const int CONFIG_VERSION = 1;

// ===== WiFi Settings =====
const char* WIFI_SSID = "LuzesAuto";      // SSID da rede Wi-Fi criada pelo ESP32
const char* WIFI_PASSWORD = "12345678";   // Senha da rede Wi-Fi
const char* AP_HOSTNAME = "luz-auto";     // Hostname da rede

// ===== Web Server Settings =====
const uint16_t WEB_SERVER_PORT = 80;
const uint32_t MAX_UPLOAD_SIZE = 1048576;  // 1MB
