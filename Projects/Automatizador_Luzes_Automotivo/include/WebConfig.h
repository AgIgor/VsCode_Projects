#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "ConfigManager.h"
#include "PinConfig.h"

// Forward declaration of HTML content
extern const char INDEX_HTML[] PROGMEM;

class WebServerManager {
private:
  WebServer* server;
  ConfigManager& configManager;
  bool wifiConnected = false;

  String getStatusJSON() {
    DynamicJsonDocument doc(2048);
    
    SystemConfig& cfg = configManager.getConfig();
    
    // Status information
    doc["connected"] = wifiConnected;
    doc["hostname"] = AP_HOSTNAME;
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["rssi"] = WiFi.RSSI();
    
    // Current input states
    doc["inputs"]["lock"] = digitalRead(cfg.pins.lock);
    doc["inputs"]["turnSignal"] = digitalRead(cfg.pins.turnSignal);
    doc["inputs"]["door"] = digitalRead(cfg.pins.door);
    doc["inputs"]["ignition"] = digitalRead(cfg.pins.ignition);
    doc["inputs"]["unlock"] = digitalRead(cfg.pins.unlock);
    doc["inputs"]["ldr"] = analogRead(cfg.pins.ldr);
    
    // Current output states
    doc["outputs"]["headlight"] = digitalRead(cfg.pins.headlight);
    doc["outputs"]["accessory"] = digitalRead(cfg.pins.accessory);
    
    String output;
    serializeJson(doc, output);
    return output;
  }

  void initRoutes() {
    // Serve HTML interface
    server->on("/", HTTP_GET, [this]() {
      server->send_P(200, "text/html", INDEX_HTML);
    });

    // API: Get current configuration
    server->on("/api/config", HTTP_GET, [this]() {
      server->send(200, "application/json", configManager.getConfigJSON());
    });

    // API: Update configuration
    server->on("/api/config", HTTP_POST, [this]() {
      String body = server->arg("plain");
      
      if (configManager.updateFromJSON(body)) {
        DynamicJsonDocument doc(256);
        doc["success"] = true;
        doc["message"] = "Configuração atualizada com sucesso";
        
        String response;
        serializeJson(doc, response);
        server->send(200, "application/json", response);
      } else {
        DynamicJsonDocument doc(256);
        doc["success"] = false;
        doc["message"] = "Erro ao atualizar configuração";
        
        String response;
        serializeJson(doc, response);
        server->send(400, "application/json", response);
      }
    });

    // API: Get system status
    server->on("/api/status", HTTP_GET, [this]() {
      server->send(200, "application/json", getStatusJSON());
    });

    // API: Reset configuration to defaults
    server->on("/api/reset", HTTP_POST, [this]() {
      SystemConfig defaultConfig;
      DynamicJsonDocument doc(4096);
      
      doc["debounceMs"] = defaultConfig.debounceMs;
      doc["signalPairWindowMs"] = defaultConfig.signalPairWindowMs;
      doc["leavingHomeLockMs"] = defaultConfig.leavingHomeLockMs;
      doc["leavingHomeUnlockMs"] = defaultConfig.leavingHomeUnlockMs;
      doc["courtesyDoorMs"] = defaultConfig.courtesyDoorMs;
      doc["followMeHomeMs"] = defaultConfig.followMeHomeMs;
      doc["courtesyDoorReararmMs"] = defaultConfig.courtesyDoorReararmMs;
      doc["ambientStableMs"] = defaultConfig.ambientStableMs;
      doc["ldrFaultStableMs"] = defaultConfig.ldrFaultStableMs;
      doc["summaryIntervalMs"] = defaultConfig.summaryIntervalMs;
      doc["accessoryOnMs"] = defaultConfig.accessoryOnMs;
      doc["ldrDarkThreshold"] = defaultConfig.ldrDarkThreshold;
      doc["ldrBrightThreshold"] = defaultConfig.ldrBrightThreshold;
      doc["ldrFaultLowRaw"] = defaultConfig.ldrFaultLowRaw;
      doc["ldrFaultHighRaw"] = defaultConfig.ldrFaultHighRaw;
      doc["invertLockInput"] = defaultConfig.invertLockInput;
      doc["invertTurnSignalInput"] = defaultConfig.invertTurnSignalInput;
      doc["invertDoorInput"] = defaultConfig.invertDoorInput;
      doc["invertIgnitionInput"] = defaultConfig.invertIgnitionInput;
      doc["invertUnlockInput"] = defaultConfig.invertUnlockInput;

      JsonObject pins = doc.createNestedObject("pins");
      pins["lock"] = defaultConfig.pins.lock;
      pins["turnSignal"] = defaultConfig.pins.turnSignal;
      pins["door"] = defaultConfig.pins.door;
      pins["ignition"] = defaultConfig.pins.ignition;
      pins["unlock"] = defaultConfig.pins.unlock;
      pins["ldr"] = defaultConfig.pins.ldr;
      pins["headlight"] = defaultConfig.pins.headlight;
      pins["accessory"] = defaultConfig.pins.accessory;

      doc["enableAutoDrive"] = defaultConfig.enableAutoDrive;
      doc["enableLeavingHome"] = defaultConfig.enableLeavingHome;
      doc["enableCourtesyDoor"] = defaultConfig.enableCourtesyDoor;
      doc["enableFollowMeHome"] = defaultConfig.enableFollowMeHome;

      String jsonStr;
      serializeJson(doc, jsonStr);
      
      if (configManager.updateFromJSON(jsonStr)) {
        DynamicJsonDocument response(256);
        response["success"] = true;
        response["message"] = "Configuração resetada para padrões";
        
        String out;
        serializeJson(response, out);
        server->send(200, "application/json", out);
      } else {
        server->send(500, "application/json", "{\"success\":false,\"message\":\"Erro ao resetar\"}");
      }
    });

    // Handle 404
    server->onNotFound([this]() {
      server->send(404, "application/json", "{\"error\":\"Rota não encontrada\"}");
    });
  }

  void setupWiFi() {
    Serial.println("\nConfigurando WiFi...");
    
    // Desabilita WiFi modo STA
    WiFi.mode(WIFI_AP_STA);
    
    // Inicia Access Point
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    
    Serial.print("Access Point iniciado: ");
    Serial.println(WIFI_SSID);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("Senha: ");
    Serial.println(WIFI_PASSWORD);
    
    wifiConnected = true;
  }

public:
  WebServerManager(ConfigManager& cfgMgr) 
    : configManager(cfgMgr) {
    server = new WebServer(WEB_SERVER_PORT);
  }
  
  ~WebServerManager() {
    delete server;
  }

  void begin() {
    setupWiFi();
    initRoutes();
    server->begin();
    Serial.println("Servidor web iniciado");
    Serial.print("Acesse: http://");
    Serial.print(WiFi.softAPIP());
    Serial.print(" ou http://");
    Serial.print(AP_HOSTNAME);
    Serial.println(".local");
  }

  void handle() {
    // Handle client requests
    server->handleClient();
  }

  bool isConnected() const { return wifiConnected; }
};

// HTML Content
const char INDEX_HTML[] PROGMEM = R"EOF(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Automatizador de Luzes Automotivo</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
        }

        header {
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            margin-bottom: 20px;
        }

        h1 {
            color: #333;
            margin-bottom: 10px;
        }

        .status-bar {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
            margin-top: 15px;
        }

        .status-item {
            background: #f5f5f5;
            padding: 10px;
            border-radius: 5px;
            font-size: 14px;
        }

        .status-item label {
            color: #666;
            display: block;
            font-size: 12px;
            margin-bottom: 5px;
        }

        .status-item .value {
            color: #333;
            font-weight: bold;
            font-size: 16px;
        }

        .tabs {
            display: flex;
            gap: 10px;
            margin-bottom: 20px;
            border-bottom: 2px solid #ddd;
            background: white;
            border-radius: 10px 10px 0 0;
            overflow: auto;
        }

        .tab-btn {
            padding: 15px 20px;
            border: none;
            background: none;
            cursor: pointer;
            font-size: 16px;
            color: #666;
            border-bottom: 3px solid transparent;
            transition: all 0.3s ease;
            white-space: nowrap;
        }

        .tab-btn.active {
            color: #667eea;
            border-bottom-color: #667eea;
        }

        .tab-btn:hover {
            background: #f5f5f5;
        }

        .tab-content {
            display: none;
            background: white;
            padding: 20px;
            border-radius: 0 10px 10px 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }

        .tab-content.active {
            display: block;
        }

        .form-group {
            margin-bottom: 20px;
        }

        .form-row {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
        }

        label {
            display: block;
            margin-bottom: 8px;
            color: #333;
            font-weight: 500;
        }

        input[type="number"],
        input[type="text"],
        select {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 14px;
            font-family: inherit;
        }

        input[type="number"]:focus,
        input[type="text"]:focus,
        select:focus {
            outline: none;
            border-color: #667eea;
            box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
        }

        .checkbox-group {
            display: flex;
            align-items: center;
            gap: 10px;
        }

        input[type="checkbox"] {
            width: 20px;
            height: 20px;
            cursor: pointer;
        }

        .form-section {
            background: #f9f9f9;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
            border-left: 4px solid #667eea;
        }

        .form-section h3 {
            color: #333;
            margin-bottom: 15px;
            font-size: 16px;
        }

        .button-group {
            display: flex;
            gap: 10px;
            margin-top: 30px;
            flex-wrap: wrap;
        }

        button {
            padding: 12px 24px;
            border: none;
            border-radius: 5px;
            font-size: 14px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
        }

        .btn-primary {
            background: #667eea;
            color: white;
        }

        .btn-primary:hover {
            background: #5568d3;
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
        }

        .btn-secondary {
            background: #e0e0e0;
            color: #333;
        }

        .btn-secondary:hover {
            background: #d0d0d0;
        }

        .btn-danger {
            background: #ff6b6b;
            color: white;
        }

        .btn-danger:hover {
            background: #ee5a52;
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(255, 107, 107, 0.4);
        }

        .alert {
            padding: 12px 15px;
            border-radius: 5px;
            margin-bottom: 20px;
            display: none;
        }

        .alert.success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
            display: block;
        }

        .alert.error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
            display: block;
        }

        .info-box {
            background: #e7f3ff;
            border-left: 4px solid #2196F3;
            padding: 12px;
            border-radius: 4px;
            margin-bottom: 15px;
            font-size: 13px;
            color: #0c5aa0;
        }

        .loading {
            display: inline-block;
            width: 16px;
            height: 16px;
            border: 2px solid #f3f3f3;
            border-top: 2px solid #667eea;
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }

        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }

        .help-text {
            font-size: 12px;
            color: #666;
            margin-top: 5px;
        }

        @media (max-width: 768px) {
            .container {
                padding: 0;
            }

            .tabs {
                border-radius: 0;
            }

            .tab-content {
                border-radius: 0;
            }

            button {
                width: 100%;
            }

            .button-group {
                flex-direction: column;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🚗 Automatizador de Luzes Automotivo</h1>
            <div class="status-bar">
                <div class="status-item">
                    <label>Status WiFi:</label>
                    <span class="value" id="wifiStatus">Conectado</span>
                </div>
                <div class="status-item">
                    <label>IP:</label>
                    <span class="value" id="ipAddress">-</span>
                </div>
                <div class="status-item">
                    <label>Uptime:</label>
                    <span class="value" id="uptime">-</span>
                </div>
                <div class="status-item">
                    <label>Memória Livre:</label>
                    <span class="value" id="freeHeap">-</span>
                </div>
            </div>
        </header>

        <div class="tabs">
            <button class="tab-btn active" onclick="switchTab('timers')">⏱️ Tempos</button>
            <button class="tab-btn" onclick="switchTab('inputs')">📥 Entradas</button>
            <button class="tab-btn" onclick="switchTab('outputs')">📤 Saídas</button>
            <button class="tab-btn" onclick="switchTab('ldr')">💡 Sensor de Luz</button>
            <button class="tab-btn" onclick="switchTab('features')">⚙️ Features</button>
            <button class="tab-btn" onclick="switchTab('system')">🔧 Sistema</button>
        </div>

        <div class="tab-content active" id="tab-timers">
            <div id="alertTimers" class="alert"></div>
            
            <div class="form-section">
                <h3>Configuração de Tempos</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label for="debounceMs">Debounce (ms)</label>
                        <input type="number" id="debounceMs" min="10" max="1000" step="10">
                        <div class="help-text">Tempo de estabilização dos sinais</div>
                    </div>
                    <div class="form-group">
                        <label for="signalPairWindowMs">Janela de Pares (ms)</label>
                        <input type="number" id="signalPairWindowMs" min="100" max="5000" step="100">
                        <div class="help-text">Tempo para reconhecer sinais em pares</div>
                    </div>
                </div>
            </div>

            <div class="form-section">
                <h3>Modo Leaving Home (Saindo de Casa)</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label for="leavingHomeLockMs">Tempo Trava (ms)</label>
                        <input type="number" id="leavingHomeLockMs" min="5000" max="120000" step="1000">
                        <div class="help-text">Duração da iluminação ao travar</div>
                    </div>
                    <div class="form-group">
                        <label for="leavingHomeUnlockMs">Tempo Destrava (ms)</label>
                        <input type="number" id="leavingHomeUnlockMs" min="5000" max="120000" step="1000">
                        <div class="help-text">Duração da iluminação ao destravar</div>
                    </div>
                </div>
            </div>

            <div class="form-section">
                <h3>Outros Tempos</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label for="courtesyDoorMs">Cortesia Porta (ms)</label>
                        <input type="number" id="courtesyDoorMs" min="5000" max="60000" step="1000">
                        <div class="help-text">Duração da iluminação ao abrir porta</div>
                    </div>
                    <div class="form-group">
                        <label for="followMeHomeMs">Follow Me Home (ms)</label>
                        <input type="number" id="followMeHomeMs" min="5000" max="120000" step="1000">
                        <div class="help-text">Duração após desligar ignição</div>
                    </div>
                    <div class="form-group">
                        <label for="courtesyDoorReararmMs">Rearme Porta (ms)</label>
                        <input type="number" id="courtesyDoorReararmMs" min="1000" max="30000" step="500">
                        <div class="help-text">Tempo para rearme automático</div>
                    </div>
                    <div class="form-group">
                        <label for="accessoryOnMs">Acessório Ligado (ms)</label>
                        <input type="number" id="accessoryOnMs" min="60000" max="3600000" step="60000">
                        <div class="help-text">Tempo do modo acessório ativo</div>
                    </div>
                </div>
            </div>

            <div class="form-section">
                <h3>Tempos de Estabilização</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label for="ambientStableMs">Ambiente Estável (ms)</label>
                        <input type="number" id="ambientStableMs" min="500" max="10000" step="100">
                        <div class="help-text">Tempo para considerar mudança de luz</div>
                    </div>
                    <div class="form-group">
                        <label for="ldrFaultStableMs">LDR Falha Estável (ms)</label>
                        <input type="number" id="ldrFaultStableMs" min="1000" max="10000" step="100">
                        <div class="help-text">Tempo para detectar falha do sensor</div>
                    </div>
                </div>
            </div>
        </div>

        <div class="tab-content" id="tab-inputs">
            <div id="alertInputs" class="alert"></div>
            
            <div class="form-section">
                <h3>Configuração de Entradas (Inputs)</h3>
                <p class="info-box">Configure os pinos GPIO e a lógica de inversão para cada entrada</p>
                
                <div class="form-row">
                    <div class="form-group">
                        <label for="lockPin">Pino Trava</label>
                        <input type="number" id="lockPin" min="0" max="39">
                        <div class="help-text">GPIO para entrada de trava</div>
                    </div>
                    <div class="form-group">
                        <label class="checkbox-group">
                            <input type="checkbox" id="invertLock">
                            <span>Inverter lógica trava</span>
                        </label>
                        <div class="help-text">Inverter LOW/HIGH</div>
                    </div>
                </div>

                <div class="form-row">
                    <div class="form-group">
                        <label for="turnSignalPin">Pino Seta</label>
                        <input type="number" id="turnSignalPin" min="0" max="39">
                        <div class="help-text">GPIO para entrada de seta</div>
                    </div>
                    <div class="form-group">
                        <label class="checkbox-group">
                            <input type="checkbox" id="invertTurnSignal">
                            <span>Inverter lógica seta</span>
                        </label>
                    </div>
                </div>

                <div class="form-row">
                    <div class="form-group">
                        <label for="doorPin">Pino Porta</label>
                        <input type="number" id="doorPin" min="0" max="39">
                        <div class="help-text">GPIO para entrada de porta</div>
                    </div>
                    <div class="form-group">
                        <label class="checkbox-group">
                            <input type="checkbox" id="invertDoor">
                            <span>Inverter lógica porta</span>
                        </label>
                    </div>
                </div>

                <div class="form-row">
                    <div class="form-group">
                        <label for="ignitionPin">Pino Ignição</label>
                        <input type="number" id="ignitionPin" min="0" max="39">
                        <div class="help-text">GPIO para entrada de ignição</div>
                    </div>
                    <div class="form-group">
                        <label class="checkbox-group">
                            <input type="checkbox" id="invertIgnition">
                            <span>Inverter lógica ignição</span>
                        </label>
                    </div>
                </div>

                <div class="form-row">
                    <div class="form-group">
                        <label for="unlockPin">Pino Destrava</label>
                        <input type="number" id="unlockPin" min="0" max="39">
                        <div class="help-text">GPIO para entrada de destrava</div>
                    </div>
                    <div class="form-group">
                        <label class="checkbox-group">
                            <input type="checkbox" id="invertUnlock">
                            <span>Inverter lógica destrava</span>
                        </label>
                    </div>
                </div>

                <div class="form-row">
                    <div class="form-group">
                        <label for="ldrPin">Pino LDR/Sensor Luz</label>
                        <input type="number" id="ldrPin" min="0" max="39">
                        <div class="help-text">GPIO ADC para sensor de luz</div>
                    </div>
                </div>
            </div>
        </div>

        <div class="tab-content" id="tab-outputs">
            <div id="alertOutputs" class="alert"></div>
            
            <div class="form-section">
                <h3>Configuração de Saídas (Outputs)</h3>
                <p class="info-box">Configure os pinos GPIO para os atuadores</p>
                
                <div class="form-row">
                    <div class="form-group">
                        <label for="headlightPin">Pino Farol</label>
                        <input type="number" id="headlightPin" min="0" max="39">
                        <div class="help-text">GPIO para controle do farol</div>
                    </div>
                </div>

                <div class="form-row">
                    <div class="form-group">
                        <label for="accessoryPin">Pino Acessório</label>
                        <input type="number" id="accessoryPin" min="0" max="39">
                        <div class="help-text">GPIO para controle do acessório</div>
                    </div>
                </div>
            </div>

            <div class="form-section">
                <h3>Estado Atual das Saídas</h3>
                <div class="form-row">
                    <div class="status-item">
                        <label>Farol:</label>
                        <span class="value" id="headlightStatus">-</span>
                    </div>
                    <div class="status-item">
                        <label>Acessório:</label>
                        <span class="value" id="accessoryStatus">-</span>
                    </div>
                </div>
            </div>
        </div>

        <div class="tab-content" id="tab-ldr">
            <div id="alertLDR" class="alert"></div>
            
            <div class="form-section">
                <h3>Configuração do Sensor de Luz (LDR)</h3>
                <p class="info-box">Ajuste os limites de detecção de luz e escuridão</p>
                
                <div class="form-row">
                    <div class="form-group">
                        <label for="ldrDarkThreshold">Limiar de Escuridão</label>
                        <input type="number" id="ldrDarkThreshold" min="0" max="4095">
                        <div class="help-text">Valor ADC abaixo de qual é considerado escuro</div>
                    </div>
                    <div class="form-group">
                        <label for="ldrBrightThreshold">Limiar de Claridade</label>
                        <input type="number" id="ldrBrightThreshold" min="0" max="4095">
                        <div class="help-text">Valor ADC acima de qual é considerado claro</div>
                    </div>
                </div>
            </div>

            <div class="form-section">
                <h3>Detecção de Falhas do LDR</h3>
                <div class="form-row">
                    <div class="form-group">
                        <label for="ldrFaultLowRaw">Valor Mínimo (Falha Aberta)</label>
                        <input type="number" id="ldrFaultLowRaw" min="0" max="100">
                        <div class="help-text">Sensor aberto tem leitura próxima de 0</div>
                    </div>
                    <div class="form-group">
                        <label for="ldrFaultHighRaw">Valor Máximo (Falha Curto)</label>
                        <input type="number" id="ldrFaultHighRaw" min="3995" max="4095">
                        <div class="help-text">Sensor em curto tem leitura próxima de 4095</div>
                    </div>
                </div>
            </div>

            <div class="form-section">
                <h3>Valor Atual do Sensor</h3>
                <div class="status-item" style="margin: 0;">
                    <label>Leitura LDR:</label>
                    <span class="value" id="ldrValue">-</span>
                </div>
            </div>
        </div>

        <div class="tab-content" id="tab-features">
            <div id="alertFeatures" class="alert"></div>
            
            <div class="form-section">
                <h3>Ativar/Desativar Features</h3>
                <p class="info-box">Controle quais modos de operação estão habilitados</p>
                
                <div class="form-row">
                    <div class="form-group">
                        <label class="checkbox-group">
                            <input type="checkbox" id="enableAutoDrive">
                            <span>Modo Auto Drive (Driving Lights)</span>
                        </label>
                        <div class="help-text">Ilumina automaticamente em ambiente escuro enquanto dirigindo</div>
                    </div>
                    <div class="form-group">
                        <label class="checkbox-group">
                            <input type="checkbox" id="enableLeavingHome">
                            <span>Modo Leaving Home</span>
                        </label>
                        <div class="help-text">Ilumina aproximação ao trancar/destravar</div>
                    </div>
                </div>

                <div class="form-row">
                    <div class="form-group">
                        <label class="checkbox-group">
                            <input type="checkbox" id="enableCourtesyDoor">
                            <span>Modo Cortesia Porta</span>
                        </label>
                        <div class="help-text">Ilumina ao abrir porta</div>
                    </div>
                    <div class="form-group">
                        <label class="checkbox-group">
                            <input type="checkbox" id="enableFollowMeHome">
                            <span>Modo Follow Me Home</span>
                        </label>
                        <div class="help-text">Mantém iluminação após desligar ignição</div>
                    </div>
                </div>
            </div>
        </div>

        <div class="tab-content" id="tab-system">
            <div id="alertSystem" class="alert"></div>
            
            <div class="form-section">
                <h3>Informações do Sistema</h3>
                <div class="form-row">
                    <div class="status-item">
                        <label>Versão do Firmware:</label>
                        <span class="value">1.0.0</span>
                    </div>
                    <div class="status-item">
                        <label>Plataforma:</label>
                        <span class="value">ESP32</span>
                    </div>
                </div>
            </div>

            <div class="form-section">
                <h3>Ações do Sistema</h3>
                <div class="button-group">
                    <button class="btn-secondary" onclick="resetSettings()">
                        Resetar para Padrões
                    </button>
                    <button class="btn-secondary" onclick="location.reload()">
                        Recarregar Página
                    </button>
                </div>
            </div>
        </div>

        <div class="button-group" style="margin-top: 20px;">
            <button class="btn-primary" onclick="saveSettings()">
                💾 Salvar Configurações
            </button>
            <button class="btn-secondary" onclick="loadSettings()">
                🔄 Carregar do Dispositivo
            </button>
        </div>
    </div>

    <script>
        // Tab switching
        function switchTab(tabName) {
            // Hide all tabs
            document.querySelectorAll('.tab-content').forEach(tab => {
                tab.classList.remove('active');
            });
            document.querySelectorAll('.tab-btn').forEach(btn => {
                btn.classList.remove('active');
            });

            // Show selected tab
            document.getElementById('tab-' + tabName).classList.add('active');
            event.target.classList.add('active');
        }

        // Show alert message
        function showAlert(tabName, message, isSuccess = true) {
            const alertEl = document.getElementById('alert' + tabName.charAt(0).toUpperCase() + tabName.slice(1));
            if (alertEl) {
                alertEl.textContent = message;
                alertEl.className = 'alert ' + (isSuccess ? 'success' : 'error');
                alertEl.style.display = 'block';
                setTimeout(() => {
                    alertEl.style.display = 'none';
                }, 5000);
            }
        }

        // Load settings from device
        async function loadSettings() {
            try {
                const response = await fetch('/api/config');
                const config = await response.json();

                // Load timing configs
                document.getElementById('debounceMs').value = config.debounceMs;
                document.getElementById('signalPairWindowMs').value = config.signalPairWindowMs;
                document.getElementById('leavingHomeLockMs').value = config.leavingHomeLockMs;
                document.getElementById('leavingHomeUnlockMs').value = config.leavingHomeUnlockMs;
                document.getElementById('courtesyDoorMs').value = config.courtesyDoorMs;
                document.getElementById('followMeHomeMs').value = config.followMeHomeMs;
                document.getElementById('courtesyDoorReararmMs').value = config.courtesyDoorReararmMs;
                document.getElementById('ambientStableMs').value = config.ambientStableMs;
                document.getElementById('ldrFaultStableMs').value = config.ldrFaultStableMs;
                document.getElementById('accessoryOnMs').value = config.accessoryOnMs;

                // Load LDR thresholds
                document.getElementById('ldrDarkThreshold').value = config.ldrDarkThreshold;
                document.getElementById('ldrBrightThreshold').value = config.ldrBrightThreshold;
                document.getElementById('ldrFaultLowRaw').value = config.ldrFaultLowRaw;
                document.getElementById('ldrFaultHighRaw').value = config.ldrFaultHighRaw;

                // Load input inversion flags
                document.getElementById('invertLock').checked = config.invertLockInput;
                document.getElementById('invertTurnSignal').checked = config.invertTurnSignalInput;
                document.getElementById('invertDoor').checked = config.invertDoorInput;
                document.getElementById('invertIgnition').checked = config.invertIgnitionInput;
                document.getElementById('invertUnlock').checked = config.invertUnlockInput;

                // Load pins
                if (config.pins) {
                    document.getElementById('lockPin').value = config.pins.lock;
                    document.getElementById('turnSignalPin').value = config.pins.turnSignal;
                    document.getElementById('doorPin').value = config.pins.door;
                    document.getElementById('ignitionPin').value = config.pins.ignition;
                    document.getElementById('unlockPin').value = config.pins.unlock;
                    document.getElementById('ldrPin').value = config.pins.ldr;
                    document.getElementById('headlightPin').value = config.pins.headlight;
                    document.getElementById('accessoryPin').value = config.pins.accessory;
                }

                // Load feature flags
                document.getElementById('enableAutoDrive').checked = config.enableAutoDrive;
                document.getElementById('enableLeavingHome').checked = config.enableLeavingHome;
                document.getElementById('enableCourtesyDoor').checked = config.enableCourtesyDoor;
                document.getElementById('enableFollowMeHome').checked = config.enableFollowMeHome;

                showAlert('timers', 'Configurações carregadas com sucesso!', true);
            } catch (error) {
                showAlert('timers', 'Erro ao carregar configurações: ' + error, false);
                console.error('Load error:', error);
            }
        }

        // Save settings to device
        async function saveSettings() {
            const config = {
                debounceMs: parseInt(document.getElementById('debounceMs').value),
                signalPairWindowMs: parseInt(document.getElementById('signalPairWindowMs').value),
                leavingHomeLockMs: parseInt(document.getElementById('leavingHomeLockMs').value),
                leavingHomeUnlockMs: parseInt(document.getElementById('leavingHomeUnlockMs').value),
                courtesyDoorMs: parseInt(document.getElementById('courtesyDoorMs').value),
                followMeHomeMs: parseInt(document.getElementById('followMeHomeMs').value),
                courtesyDoorReararmMs: parseInt(document.getElementById('courtesyDoorReararmMs').value),
                ambientStableMs: parseInt(document.getElementById('ambientStableMs').value),
                ldrFaultStableMs: parseInt(document.getElementById('ldrFaultStableMs').value),
                accessoryOnMs: parseInt(document.getElementById('accessoryOnMs').value),
                ldrDarkThreshold: parseInt(document.getElementById('ldrDarkThreshold').value),
                ldrBrightThreshold: parseInt(document.getElementById('ldrBrightThreshold').value),
                ldrFaultLowRaw: parseInt(document.getElementById('ldrFaultLowRaw').value),
                ldrFaultHighRaw: parseInt(document.getElementById('ldrFaultHighRaw').value),
                invertLockInput: document.getElementById('invertLock').checked,
                invertTurnSignalInput: document.getElementById('invertTurnSignal').checked,
                invertDoorInput: document.getElementById('invertDoor').checked,
                invertIgnitionInput: document.getElementById('invertIgnition').checked,
                invertUnlockInput: document.getElementById('invertUnlock').checked,
                pins: {
                    lock: parseInt(document.getElementById('lockPin').value),
                    turnSignal: parseInt(document.getElementById('turnSignalPin').value),
                    door: parseInt(document.getElementById('doorPin').value),
                    ignition: parseInt(document.getElementById('ignitionPin').value),
                    unlock: parseInt(document.getElementById('unlockPin').value),
                    ldr: parseInt(document.getElementById('ldrPin').value),
                    headlight: parseInt(document.getElementById('headlightPin').value),
                    accessory: parseInt(document.getElementById('accessoryPin').value)
                },
                enableAutoDrive: document.getElementById('enableAutoDrive').checked,
                enableLeavingHome: document.getElementById('enableLeavingHome').checked,
                enableCourtesyDoor: document.getElementById('enableCourtesyDoor').checked,
                enableFollowMeHome: document.getElementById('enableFollowMeHome').checked
            };

            try {
                const response = await fetch('/api/config', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(config)
                });

                const result = await response.json();

                if (result.success) {
                    showAlert('timers', 'Configurações salvas com sucesso!', true);
                } else {
                    showAlert('timers', 'Erro ao salvar: ' + result.message, false);
                }
            } catch (error) {
                showAlert('timers', 'Erro de conexão: ' + error, false);
                console.error('Save error:', error);
            }
        }

        // Reset to defaults
        async function resetSettings() {
            if (confirm('Tem certeza que deseja resetar todas as configurações para os padrões?')) {
                try {
                    const response = await fetch('/api/reset', {
                        method: 'POST'
                    });

                    const result = await response.json();

                    if (result.success) {
                        setTimeout(() => {
                            loadSettings();
                            showAlert('system', result.message, true);
                        }, 500);
                    } else {
                        showAlert('system', result.message, false);
                    }
                } catch (error) {
                    showAlert('system', 'Erro ao resetar: ' + error, false);
                }
            }
        }

        // Update status
        async function updateStatus() {
            try {
                const response = await fetch('/api/status');
                const status = await response.json();

                document.getElementById('wifiStatus').textContent = status.connected ? 'Conectado' : 'Desconectado';
                document.getElementById('uptime').textContent = Math.floor(status.uptime) + 's';
                document.getElementById('freeHeap').textContent = (status.freeHeap / 1024).toFixed(1) + ' KB';
                document.getElementById('ldrValue').textContent = status.inputs.ldr;
                document.getElementById('headlightStatus').textContent = status.outputs.headlight ? 'LIGADO' : 'DESLIGADO';
                document.getElementById('accessoryStatus').textContent = status.outputs.accessory ? 'LIGADO' : 'DESLIGADO';
            } catch (error) {
                console.error('Status update error:', error);
            }
        }

        // Initialize
        window.addEventListener('load', function() {
            loadSettings();
            updateStatus();
            setInterval(updateStatus, 2000);
        });
    </script>
</body>
</html>
)EOF";
