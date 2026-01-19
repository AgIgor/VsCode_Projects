#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ============================================
// DEFINIÇÕES DE PINOS
// ============================================

// INPUTS (PULL-UP)
#define PIN_DOOR_OPEN       32  // Porta aberta (LOW = aberto)
#define PIN_LOCK_SIGNAL     33  // Sinal de trava (LOW = travando)
#define PIN_UNLOCK_SIGNAL   13  // Sinal de destrava (LOW = destravando)
#define PIN_IGNITION        25  // Pós-chave/Ignição (LOW = ligado)

// OUTPUTS (DIGITAL)
#define PIN_HEADLIGHT       26  // Farol baixo
#define PIN_DRL             27  // DRL (Daytime Running Lights)
#define PIN_INTERIOR_LIGHT  14  // Luz interna
#define PIN_FOOT_LIGHT      12  // Luz dos pés

// ============================================
// CONFIGURAÇÕES WiFi
// ============================================
const char* AP_SSID = "BCM_ESP32_Config";
const char* AP_PASS = "12345678";

// ============================================
// ESTRUTURA DE CONFIGURAÇÃO
// ============================================
struct BCMConfig {
  // Tempos (em segundos)
  uint16_t welcomeLightDuration;     
  uint16_t goodbyeLightDuration;     
  uint16_t followMeHomeDuration;     
  uint16_t doorOpenLightDuration;    
  uint16_t maxOnTime;                
  
  // === EVENTO: DESTRAVAR (Welcome Light) ===
  bool unlockEnable;
  bool unlockHeadlight;
  bool unlockDRL;
  bool unlockInterior;
  bool unlockFoot;
  
  // === EVENTO: TRAVAR (Follow Me Home) ===
  bool lockEnable;
  bool lockHeadlight;
  bool lockDRL;
  bool lockInterior;
  bool lockFoot;
  
  // === EVENTO: IGNIÇÃO LIGADA ===
  bool ignitionOnEnable;
  bool ignitionOnHeadlight;
  bool ignitionOnDRL;
  bool ignitionOnInterior;
  bool ignitionOnFoot;
  
  // === EVENTO: IGNIÇÃO DESLIGADA (Goodbye Light) ===
  bool ignitionOffEnable;
  bool ignitionOffHeadlight;
  bool ignitionOffDRL;
  bool ignitionOffInterior;
  bool ignitionOffFoot;
  
  // === EVENTO: PORTA ABERTA ===
  bool doorOpenEnable;
  bool doorOpenHeadlight;
  bool doorOpenDRL;
  bool doorOpenInterior;
  bool doorOpenFoot;
};

// ============================================
// VARIÁVEIS GLOBAIS
// ============================================
BCMConfig config;
Preferences preferences;
WebServer server(80);

// Estados dos inputs
bool doorOpen = false;
bool doorOpenPrev = false;
bool lockSignal = false;
bool lockSignalPrev = false;
bool unlockSignal = false;
bool unlockSignalPrev = false;
bool ignitionOn = false;
bool ignitionOnPrev = false;

// Debounce para sinais
#define DEBOUNCE_SAMPLES 5
unsigned long lastDebounceTime = 0;
const int DEBOUNCE_DELAY = 20; // 20ms

// Buffers de debounce para trava e destrava
uint8_t lockDebounceBuffer[DEBOUNCE_SAMPLES] = {1, 1, 1, 1, 1};
uint8_t unlockDebounceBuffer[DEBOUNCE_SAMPLES] = {1, 1, 1, 1, 1};
int debounceIndex = 0;

// Controle de timers
unsigned long welcomeLightTimer = 0;
unsigned long goodbyeLightTimer = 0;
unsigned long followMeHomeTimer = 0;
unsigned long doorLightTimer = 0;
unsigned long systemOnTimer = 0;

bool welcomeLightActive = false;
bool goodbyeLightActive = false;
bool followMeHomeActive = false;
bool doorLightActive = false;

// Log de eventos
#define MAX_LOG_ENTRIES 50
String eventLog[MAX_LOG_ENTRIES];
int logIndex = 0;
int logCount = 0;

// Saídas atuais
bool headlightOn = false;
bool drlOn = false;
bool interiorOn = false;
bool footOn = false;

// Proteção de timeout
bool timeoutTriggered = false;

// ============================================
// PÁGINA WEB EM PROGMEM
// ============================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang='pt-BR'>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>BCM ESP32 Config</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 20px;
            min-height: 100vh;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            border-radius: 20px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
        }
        .header p {
            opacity: 0.9;
            font-size: 1.1em;
        }
        .content {
            padding: 30px;
        }
        .section {
            margin-bottom: 30px;
            padding: 20px;
            background: #f8f9fa;
            border-radius: 10px;
            border-left: 4px solid #667eea;
        }
        .section h2 {
            color: #667eea;
            margin-bottom: 20px;
            font-size: 1.5em;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: #333;
            font-weight: 600;
        }
        input[type='number'] {
            width: 100%;
            padding: 12px;
            border: 2px solid #ddd;
            border-radius: 8px;
            font-size: 16px;
            transition: border-color 0.3s;
        }
        input[type='number']:focus {
            outline: none;
            border-color: #667eea;
        }
        .checkbox-group {
            display: flex;
            align-items: center;
            padding: 12px;
            background: white;
            border-radius: 8px;
            margin-bottom: 10px;
        }
        input[type='checkbox'] {
            width: 20px;
            height: 20px;
            margin-right: 10px;
            cursor: pointer;
        }
        .checkbox-group label {
            margin: 0;
            cursor: pointer;
            font-weight: 500;
        }
        .button-group {
            display: flex;
            gap: 15px;
            margin-top: 30px;
        }
        button {
            flex: 1;
            padding: 15px;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(0,0,0,0.2);
        }
        .btn-save {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }
        .btn-load {
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
            color: white;
        }
        .status {
            padding: 15px;
            margin-top: 20px;
            border-radius: 8px;
            text-align: center;
            font-weight: bold;
            display: none;
        }
        .status.success {
            background: #d4edda;
            color: #155724;
            display: block;
        }
        .status.error {
            background: #f8d7da;
            color: #721c24;
            display: block;
        }
        .info-box {
            background: #e7f3ff;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #2196F3;
            margin-bottom: 20px;
        }
        .info-box p {
            color: #1976D2;
            line-height: 1.6;
        }
        .status-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        .status-item {
            padding: 12px;
            background: white;
            border-radius: 8px;
            border-left: 4px solid #667eea;
            font-size: 14px;
        }
        .status-item strong {
            display: block;
            color: #667eea;
            margin-bottom: 5px;
        }
        .status-value {
            font-size: 16px;
            font-weight: bold;
            color: #333;
        }
        .status-on {
            color: #28a745;
        }
        .status-off {
            color: #dc3545;
        }
        .log-box {
            background: #1e1e1e;
            color: #00ff00;
            padding: 15px;
            border-radius: 8px;
            font-family: 'Courier New', monospace;
            font-size: 12px;
            height: 250px;
            overflow-y: auto;
            border: 1px solid #333;
            margin-top: 10px;
        }
        .log-entry {
            margin-bottom: 5px;
            padding: 3px 0;
            border-bottom: 1px solid #333;
        }
        .log-entry.warning {
            color: #ffaa00;
        }
        .log-entry.error {
            color: #ff4444;
        }
    </style>
</head>
<body>
    <div class='container'>
        <div class='header'>
            <h1>🚗 BCM ESP32</h1>
            <p>Body Control Module - Controle Inteligente de Iluminação</p>
        </div>
        
        <div class='content'>
            <div class='info-box'>
                <p><strong>Status:</strong> Sistema Operacional | <strong>IP:</strong> <span id='ip'></span></p>
            </div>

            <div class='section'>
                <h2>📊 Status em Tempo Real</h2>
                <div class='status-grid'>
                    <div class='status-item'>
                        <strong>🚪 Porta</strong>
                        <div class='status-value' id='doorStatus'>FECHADA</div>
                    </div>
                    <div class='status-item'>
                        <strong>🔒 Trava</strong>
                        <div class='status-value' id='lockStatus'>INATIVO</div>
                    </div>
                    <div class='status-item'>
                        <strong>🔓 Destrava</strong>
                        <div class='status-value' id='unlockStatus'>INATIVO</div>
                    </div>
                    <div class='status-item'>
                        <strong>🔑 Ignição</strong>
                        <div class='status-value' id='ignitionStatus'>DESLIGADA</div>
                    </div>
                    <div class='status-item'>
                        <strong>💡 Farois</strong>
                        <div class='status-value' id='headlightStatus'>DESLIGADO</div>
                    </div>
                    <div class='status-item'>
                        <strong>☀️ DRL</strong>
                        <div class='status-value' id='drlStatus'>DESLIGADO</div>
                    </div>
                    <div class='status-item'>
                        <strong>🏠 Luz Interna</strong>
                        <div class='status-value' id='interiorStatus'>DESLIGADA</div>
                    </div>
                    <div class='status-item'>
                        <strong>👣 Luz Pés</strong>
                        <div class='status-value' id='footStatus'>DESLIGADA</div>
                    </div>
                </div>
                <div>
                    <strong style='color: #667eea;'>📜 Log de Eventos:</strong>
                    <div class='log-box' id='logBox'>
                        <div class='log-entry'>Conectado...</div>
                    </div>
                </div>
            </div>

            <div class='section'>
                <h2>⏱️ Tempos de Duração (segundos)</h2>
                <div class='form-group'>
                    <label>Welcome Light (ao destravar):</label>
                    <input type='number' id='welcomeDuration' min='5' max='300' value='30'>
                </div>
                <div class='form-group'>
                    <label>Goodbye Light (ao desligar ignição):</label>
                    <input type='number' id='goodbyeDuration' min='5' max='300' value='30'>
                </div>
                <div class='form-group'>
                    <label>Follow Me Home (ao travar):</label>
                    <input type='number' id='followMeDuration' min='5' max='300' value='60'>
                </div>
                <div class='form-group'>
                    <label>Luz ao Abrir Porta:</label>
                    <input type='number' id='doorDuration' min='5' max='300' value='20'>
                </div>
                <div class='form-group'>
                    <label>Tempo Máximo Ligado (proteção bateria):</label>
                    <input type='number' id='maxOnTime' min='60' max='3600' value='600'>
                </div>
            </div>

            <div class='section'>
                <h2>� DESTRAVAR - Welcome Light</h2>
                <div class='checkbox-group'>
                    <input type='checkbox' id='unlockEnable' checked>
                    <label for='unlockEnable'><strong>Ativar Welcome Light</strong></label>
                </div>
                <div style='margin-left: 30px;'>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='unlockHeadlight' checked>
                        <label for='unlockHeadlight'>💡 Ligar Farois</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='unlockDRL' checked>
                        <label for='unlockDRL'>☀️ Ligar DRL</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='unlockInterior' checked>
                        <label for='unlockInterior'>🏠 Ligar Luz Interna</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='unlockFoot' checked>
                        <label for='unlockFoot'>👣 Ligar Luz Pés</label>
                    </div>
                </div>
            </div>

            <div class='section'>
                <h2>🔒 TRAVAR - Follow Me Home</h2>
                <div class='checkbox-group'>
                    <input type='checkbox' id='lockEnable' checked>
                    <label for='lockEnable'><strong>Ativar Follow Me Home</strong></label>
                </div>
                <div style='margin-left: 30px;'>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='lockHeadlight' checked>
                        <label for='lockHeadlight'>💡 Ligar Farois</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='lockDRL'>
                        <label for='lockDRL'>☀️ Ligar DRL</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='lockInterior' checked>
                        <label for='lockInterior'>🏠 Ligar Luz Interna</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='lockFoot' checked>
                        <label for='lockFoot'>👣 Ligar Luz Pés</label>
                    </div>
                </div>
            </div>

            <div class='section'>
                <h2>🔑 LIGAR IGNIÇÃO</h2>
                <div class='checkbox-group'>
                    <input type='checkbox' id='ignitionOnEnable' checked>
                    <label for='ignitionOnEnable'><strong>Ativar Luzes ao Ligar</strong></label>
                </div>
                <div style='margin-left: 30px;'>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='ignitionOnHeadlight' checked>
                        <label for='ignitionOnHeadlight'>💡 Ligar Farois</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='ignitionOnDRL' checked>
                        <label for='ignitionOnDRL'>☀️ Ligar DRL</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='ignitionOnInterior'>
                        <label for='ignitionOnInterior'>🏠 Ligar Luz Interna</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='ignitionOnFoot'>
                        <label for='ignitionOnFoot'>👣 Ligar Luz Pés</label>
                    </div>
                </div>
            </div>

            <div class='section'>
                <h2>⚫ DESLIGAR IGNIÇÃO - Goodbye Light</h2>
                <div class='checkbox-group'>
                    <input type='checkbox' id='ignitionOffEnable' checked>
                    <label for='ignitionOffEnable'><strong>Ativar Goodbye Light</strong></label>
                </div>
                <div style='margin-left: 30px;'>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='ignitionOffHeadlight' checked>
                        <label for='ignitionOffHeadlight'>💡 Ligar Farois</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='ignitionOffDRL'>
                        <label for='ignitionOffDRL'>☀️ Ligar DRL</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='ignitionOffInterior'>
                        <label for='ignitionOffInterior'>🏠 Ligar Luz Interna</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='ignitionOffFoot'>
                        <label for='ignitionOffFoot'>👣 Ligar Luz Pés</label>
                    </div>
                </div>
            </div>

            <div class='section'>
                <h2>🚪 ABRIR PORTA</h2>
                <div class='checkbox-group'>
                    <input type='checkbox' id='doorOpenEnable' checked>
                    <label for='doorOpenEnable'><strong>Ativar Luzes</strong></label>
                </div>
                <div style='margin-left: 30px;'>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='doorOpenHeadlight'>
                        <label for='doorOpenHeadlight'>💡 Ligar Farois</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='doorOpenDRL'>
                        <label for='doorOpenDRL'>☀️ Ligar DRL</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='doorOpenInterior' checked>
                        <label for='doorOpenInterior'>🏠 Ligar Luz Interna</label>
                    </div>
                    <div class='checkbox-group'>
                        <input type='checkbox' id='doorOpenFoot' checked>
                        <label for='doorOpenFoot'>👣 Ligar Luz Pés</label>
                    </div>
                </div>
            </div>

            <div class='button-group'>
                <button class='btn-load' onclick='loadConfig()'>🔄 Carregar Configurações</button>
                <button class='btn-save' onclick='saveConfig()'>💾 Salvar Configurações</button>
            </div>

            <div id='status' class='status'></div>
        </div>
    </div>

    <script>
        // Carregar IP
        document.getElementById('ip').textContent = window.location.hostname;

        function showStatus(message, isSuccess) {
            const status = document.getElementById('status');
            status.textContent = message;
            status.className = 'status ' + (isSuccess ? 'success' : 'error');
            setTimeout(() => {
                status.style.display = 'none';
            }, 3000);
        }

        function loadConfig() {
            fetch('/getConfig')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('welcomeDuration').value = data.welcomeLightDuration;
                    document.getElementById('goodbyeDuration').value = data.goodbyeLightDuration;
                    document.getElementById('followMeDuration').value = data.followMeHomeDuration;
                    document.getElementById('doorDuration').value = data.doorOpenLightDuration;
                    document.getElementById('maxOnTime').value = data.maxOnTime;
                    
                    document.getElementById('unlockEnable').checked = data.unlockEnable;
                    document.getElementById('unlockHeadlight').checked = data.unlockHeadlight;
                    document.getElementById('unlockDRL').checked = data.unlockDRL;
                    document.getElementById('unlockInterior').checked = data.unlockInterior;
                    document.getElementById('unlockFoot').checked = data.unlockFoot;
                    
                    document.getElementById('lockEnable').checked = data.lockEnable;
                    document.getElementById('lockHeadlight').checked = data.lockHeadlight;
                    document.getElementById('lockDRL').checked = data.lockDRL;
                    document.getElementById('lockInterior').checked = data.lockInterior;
                    document.getElementById('lockFoot').checked = data.lockFoot;
                    
                    document.getElementById('ignitionOnEnable').checked = data.ignitionOnEnable;
                    document.getElementById('ignitionOnHeadlight').checked = data.ignitionOnHeadlight;
                    document.getElementById('ignitionOnDRL').checked = data.ignitionOnDRL;
                    document.getElementById('ignitionOnInterior').checked = data.ignitionOnInterior;
                    document.getElementById('ignitionOnFoot').checked = data.ignitionOnFoot;
                    
                    document.getElementById('ignitionOffEnable').checked = data.ignitionOffEnable;
                    document.getElementById('ignitionOffHeadlight').checked = data.ignitionOffHeadlight;
                    document.getElementById('ignitionOffDRL').checked = data.ignitionOffDRL;
                    document.getElementById('ignitionOffInterior').checked = data.ignitionOffInterior;
                    document.getElementById('ignitionOffFoot').checked = data.ignitionOffFoot;
                    
                    document.getElementById('doorOpenEnable').checked = data.doorOpenEnable;
                    document.getElementById('doorOpenHeadlight').checked = data.doorOpenHeadlight;
                    document.getElementById('doorOpenDRL').checked = data.doorOpenDRL;
                    document.getElementById('doorOpenInterior').checked = data.doorOpenInterior;
                    document.getElementById('doorOpenFoot').checked = data.doorOpenFoot;
                    
                    showStatus('✅ Configurações carregadas com sucesso!', true);
                })
                .catch(err => {
                    showStatus('❌ Erro ao carregar configurações!', false);
                });
        }

        function saveConfig() {
            const config = {
                welcomeLightDuration: parseInt(document.getElementById('welcomeDuration').value),
                goodbyeLightDuration: parseInt(document.getElementById('goodbyeDuration').value),
                followMeHomeDuration: parseInt(document.getElementById('followMeDuration').value),
                doorOpenLightDuration: parseInt(document.getElementById('doorDuration').value),
                maxOnTime: parseInt(document.getElementById('maxOnTime').value),
                
                unlockEnable: document.getElementById('unlockEnable').checked,
                unlockHeadlight: document.getElementById('unlockHeadlight').checked,
                unlockDRL: document.getElementById('unlockDRL').checked,
                unlockInterior: document.getElementById('unlockInterior').checked,
                unlockFoot: document.getElementById('unlockFoot').checked,
                
                lockEnable: document.getElementById('lockEnable').checked,
                lockHeadlight: document.getElementById('lockHeadlight').checked,
                lockDRL: document.getElementById('lockDRL').checked,
                lockInterior: document.getElementById('lockInterior').checked,
                lockFoot: document.getElementById('lockFoot').checked,
                
                ignitionOnEnable: document.getElementById('ignitionOnEnable').checked,
                ignitionOnHeadlight: document.getElementById('ignitionOnHeadlight').checked,
                ignitionOnDRL: document.getElementById('ignitionOnDRL').checked,
                ignitionOnInterior: document.getElementById('ignitionOnInterior').checked,
                ignitionOnFoot: document.getElementById('ignitionOnFoot').checked,
                
                ignitionOffEnable: document.getElementById('ignitionOffEnable').checked,
                ignitionOffHeadlight: document.getElementById('ignitionOffHeadlight').checked,
                ignitionOffDRL: document.getElementById('ignitionOffDRL').checked,
                ignitionOffInterior: document.getElementById('ignitionOffInterior').checked,
                ignitionOffFoot: document.getElementById('ignitionOffFoot').checked,
                
                doorOpenEnable: document.getElementById('doorOpenEnable').checked,
                doorOpenHeadlight: document.getElementById('doorOpenHeadlight').checked,
                doorOpenDRL: document.getElementById('doorOpenDRL').checked,
                doorOpenInterior: document.getElementById('doorOpenInterior').checked,
                doorOpenFoot: document.getElementById('doorOpenFoot').checked
            };

            fetch('/saveConfig', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(config)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    showStatus('✅ Configurações salvas com sucesso!', true);
                } else {
                    showStatus('❌ Erro ao salvar configurações!', false);
                }
            })
            .catch(err => {
                showStatus('❌ Erro ao salvar configurações!', false);
            });
        }

        // Carregar configurações ao iniciar
        window.onload = () => {
            loadConfig();
            updateStatus();
            setInterval(updateStatus, 500); // Atualizar a cada 500ms
        };

        function updateStatus() {
            fetch('/getStatus')
                .then(response => response.json())
                .then(data => {
                    // Atualizar status das entradas
                    document.getElementById('doorStatus').textContent = data.doorOpen ? '🔴 ABERTA' : '🟢 FECHADA';
                    document.getElementById('doorStatus').className = 'status-value ' + (data.doorOpen ? 'status-on' : 'status-off');
                    
                    document.getElementById('lockStatus').textContent = data.lockSignal ? '🔴 ATIVO' : '🟢 INATIVO';
                    document.getElementById('lockStatus').className = 'status-value ' + (data.lockSignal ? 'status-on' : 'status-off');
                    
                    document.getElementById('unlockStatus').textContent = data.unlockSignal ? '🔴 ATIVO' : '🟢 INATIVO';
                    document.getElementById('unlockStatus').className = 'status-value ' + (data.unlockSignal ? 'status-on' : 'status-off');
                    
                    document.getElementById('ignitionStatus').textContent = data.ignitionOn ? '🔴 LIGADA' : '🟢 DESLIGADA';
                    document.getElementById('ignitionStatus').className = 'status-value ' + (data.ignitionOn ? 'status-on' : 'status-off');
                    
                    // Atualizar status das saídas
                    document.getElementById('headlightStatus').textContent = data.headlight ? '🔴 LIGADO' : '🟢 DESLIGADO';
                    document.getElementById('headlightStatus').className = 'status-value ' + (data.headlight ? 'status-on' : 'status-off');
                    
                    document.getElementById('drlStatus').textContent = data.drl ? '🔴 LIGADO' : '🟢 DESLIGADO';
                    document.getElementById('drlStatus').className = 'status-value ' + (data.drl ? 'status-on' : 'status-off');
                    
                    document.getElementById('interiorStatus').textContent = data.interior ? '🔴 LIGADA' : '🟢 DESLIGADA';
                    document.getElementById('interiorStatus').className = 'status-value ' + (data.interior ? 'status-on' : 'status-off');
                    
                    document.getElementById('footStatus').textContent = data.foot ? '🔴 LIGADA' : '🟢 DESLIGADA';
                    document.getElementById('footStatus').className = 'status-value ' + (data.foot ? 'status-on' : 'status-off');
                    
                    // Atualizar log
                    const logBox = document.getElementById('logBox');
                    logBox.innerHTML = '';
                    data.logs.forEach(log => {
                        const entry = document.createElement('div');
                        entry.className = 'log-entry';
                        if (log.includes('PROTEÇÃO')) entry.className += ' error';
                        if (log.includes('finalizado')) entry.className += ' warning';
                        entry.textContent = log;
                        logBox.appendChild(entry);
                    });
                    logBox.scrollTop = logBox.scrollHeight;
                })
                .catch(err => console.log('Erro ao atualizar status'));
        }
    </script>
</body>
</html>
)rawliteral";

// ============================================
// FUNÇÕES AUXILIARES
// ============================================

void addLog(String message) {
  eventLog[logIndex] = message;
  logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
  if (logCount < MAX_LOG_ENTRIES) {
    logCount++;
  }
  Serial.println(message);
}

void loadDefaultConfig() {
  config.welcomeLightDuration = 30;
  config.goodbyeLightDuration = 30;
  config.followMeHomeDuration = 60;
  config.doorOpenLightDuration = 20;
  config.maxOnTime = 600;
  
  // Destravar
  config.unlockEnable = true;
  config.unlockHeadlight = true;
  config.unlockDRL = true;
  config.unlockInterior = true;
  config.unlockFoot = true;
  
  // Travar
  config.lockEnable = true;
  config.lockHeadlight = true;
  config.lockDRL = false;
  config.lockInterior = true;
  config.lockFoot = true;
  
  // Ignição ligada
  config.ignitionOnEnable = true;
  config.ignitionOnHeadlight = true;
  config.ignitionOnDRL = true;
  config.ignitionOnInterior = false;
  config.ignitionOnFoot = false;
  
  // Ignição desligada
  config.ignitionOffEnable = true;
  config.ignitionOffHeadlight = true;
  config.ignitionOffDRL = false;
  config.ignitionOffInterior = false;
  config.ignitionOffFoot = false;
  
  // Porta aberta
  config.doorOpenEnable = true;
  config.doorOpenHeadlight = false;
  config.doorOpenDRL = false;
  config.doorOpenInterior = true;
  config.doorOpenFoot = true;
}

void loadConfig() {
  preferences.begin("bcm-config", false);
  
  config.welcomeLightDuration = preferences.getUShort("welcome", 30);
  config.goodbyeLightDuration = preferences.getUShort("goodbye", 30);
  config.followMeHomeDuration = preferences.getUShort("followme", 60);
  config.doorOpenLightDuration = preferences.getUShort("door", 20);
  config.maxOnTime = preferences.getUShort("maxtime", 600);
  
  config.unlockEnable = preferences.getBool("ue", true);
  config.unlockHeadlight = preferences.getBool("uh", true);
  config.unlockDRL = preferences.getBool("ud", true);
  config.unlockInterior = preferences.getBool("ui", true);
  config.unlockFoot = preferences.getBool("uf", true);
  
  config.lockEnable = preferences.getBool("le", true);
  config.lockHeadlight = preferences.getBool("lh", true);
  config.lockDRL = preferences.getBool("ld", false);
  config.lockInterior = preferences.getBool("li", true);
  config.lockFoot = preferences.getBool("lf", true);
  
  config.ignitionOnEnable = preferences.getBool("ioe", true);
  config.ignitionOnHeadlight = preferences.getBool("ioh", true);
  config.ignitionOnDRL = preferences.getBool("iod", true);
  config.ignitionOnInterior = preferences.getBool("ioI", false);
  config.ignitionOnFoot = preferences.getBool("iof", false);
  
  config.ignitionOffEnable = preferences.getBool("iee", true);
  config.ignitionOffHeadlight = preferences.getBool("ieh", true);
  config.ignitionOffDRL = preferences.getBool("ied", false);
  config.ignitionOffInterior = preferences.getBool("ieI", false);
  config.ignitionOffFoot = preferences.getBool("ief", false);
  
  config.doorOpenEnable = preferences.getBool("doe", true);
  config.doorOpenHeadlight = preferences.getBool("doh", false);
  config.doorOpenDRL = preferences.getBool("dod", false);
  config.doorOpenInterior = preferences.getBool("doI", true);
  config.doorOpenFoot = preferences.getBool("dof", true);
  
  preferences.end();
  
  Serial.println("Configuração carregada!");
}

void saveConfig() {
  preferences.begin("bcm-config", false);
  
  preferences.putUShort("welcome", config.welcomeLightDuration);
  preferences.putUShort("goodbye", config.goodbyeLightDuration);
  preferences.putUShort("followme", config.followMeHomeDuration);
  preferences.putUShort("door", config.doorOpenLightDuration);
  preferences.putUShort("maxtime", config.maxOnTime);
  
  preferences.putBool("ue", config.unlockEnable);
  preferences.putBool("uh", config.unlockHeadlight);
  preferences.putBool("ud", config.unlockDRL);
  preferences.putBool("ui", config.unlockInterior);
  preferences.putBool("uf", config.unlockFoot);
  
  preferences.putBool("le", config.lockEnable);
  preferences.putBool("lh", config.lockHeadlight);
  preferences.putBool("ld", config.lockDRL);
  preferences.putBool("li", config.lockInterior);
  preferences.putBool("lf", config.lockFoot);
  
  preferences.putBool("ioe", config.ignitionOnEnable);
  preferences.putBool("ioh", config.ignitionOnHeadlight);
  preferences.putBool("iod", config.ignitionOnDRL);
  preferences.putBool("ioI", config.ignitionOnInterior);
  preferences.putBool("iof", config.ignitionOnFoot);
  
  preferences.putBool("iee", config.ignitionOffEnable);
  preferences.putBool("ieh", config.ignitionOffHeadlight);
  preferences.putBool("ied", config.ignitionOffDRL);
  preferences.putBool("ieI", config.ignitionOffInterior);
  preferences.putBool("ief", config.ignitionOffFoot);
  
  preferences.putBool("doe", config.doorOpenEnable);
  preferences.putBool("doh", config.doorOpenHeadlight);
  preferences.putBool("dod", config.doorOpenDRL);
  preferences.putBool("doI", config.doorOpenInterior);
  preferences.putBool("dof", config.doorOpenFoot);
  
  preferences.end();
  
  Serial.println("Configuração salva!");
}

// ============================================
// HANDLERS DO SERVIDOR WEB
// ============================================

String extractJsonValue(String json, String key) {
  String searchKey = "\"" + key + "\":";
  int startIndex = json.indexOf(searchKey);
  if (startIndex == -1) return "";
  
  startIndex += searchKey.length();
  int endIndex = json.indexOf(",", startIndex);
  if (endIndex == -1) endIndex = json.indexOf("}", startIndex);
  
  String value = json.substring(startIndex, endIndex);
  value.trim();
  value.replace("\"", "");
  return value;
}

void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleGetStatus() {
  String json = "{";
  json += "\"doorOpen\":" + String(doorOpen ? "true" : "false") + ",";
  json += "\"lockSignal\":" + String(lockSignal ? "true" : "false") + ",";
  json += "\"unlockSignal\":" + String(unlockSignal ? "true" : "false") + ",";
  json += "\"ignitionOn\":" + String(ignitionOn ? "true" : "false") + ",";
  json += "\"headlight\":" + String(headlightOn ? "true" : "false") + ",";
  json += "\"drl\":" + String(drlOn ? "true" : "false") + ",";
  json += "\"interior\":" + String(interiorOn ? "true" : "false") + ",";
  json += "\"foot\":" + String(footOn ? "true" : "false") + ",";
  json += "\"logs\":[";
  
  // Adicionar últimos logs
  int startIdx = logCount < MAX_LOG_ENTRIES ? 0 : logIndex;
  for (int i = 0; i < logCount; i++) {
    int idx = (startIdx + i) % MAX_LOG_ENTRIES;
    json += "\"" + eventLog[idx] + "\"";
    if (i < logCount - 1) json += ",";
  }
  
  json += "]}";
  
  server.send(200, "application/json", json);
}

void handleGetConfig() {
  String json = "{";
  json += "\"welcomeLightDuration\":" + String(config.welcomeLightDuration) + ",";
  json += "\"goodbyeLightDuration\":" + String(config.goodbyeLightDuration) + ",";
  json += "\"followMeHomeDuration\":" + String(config.followMeHomeDuration) + ",";
  json += "\"doorOpenLightDuration\":" + String(config.doorOpenLightDuration) + ",";
  json += "\"maxOnTime\":" + String(config.maxOnTime) + ",";
  
  json += "\"unlockEnable\":" + String(config.unlockEnable ? "true" : "false") + ",";
  json += "\"unlockHeadlight\":" + String(config.unlockHeadlight ? "true" : "false") + ",";
  json += "\"unlockDRL\":" + String(config.unlockDRL ? "true" : "false") + ",";
  json += "\"unlockInterior\":" + String(config.unlockInterior ? "true" : "false") + ",";
  json += "\"unlockFoot\":" + String(config.unlockFoot ? "true" : "false") + ",";
  
  json += "\"lockEnable\":" + String(config.lockEnable ? "true" : "false") + ",";
  json += "\"lockHeadlight\":" + String(config.lockHeadlight ? "true" : "false") + ",";
  json += "\"lockDRL\":" + String(config.lockDRL ? "true" : "false") + ",";
  json += "\"lockInterior\":" + String(config.lockInterior ? "true" : "false") + ",";
  json += "\"lockFoot\":" + String(config.lockFoot ? "true" : "false") + ",";
  
  json += "\"ignitionOnEnable\":" + String(config.ignitionOnEnable ? "true" : "false") + ",";
  json += "\"ignitionOnHeadlight\":" + String(config.ignitionOnHeadlight ? "true" : "false") + ",";
  json += "\"ignitionOnDRL\":" + String(config.ignitionOnDRL ? "true" : "false") + ",";
  json += "\"ignitionOnInterior\":" + String(config.ignitionOnInterior ? "true" : "false") + ",";
  json += "\"ignitionOnFoot\":" + String(config.ignitionOnFoot ? "true" : "false") + ",";
  
  json += "\"ignitionOffEnable\":" + String(config.ignitionOffEnable ? "true" : "false") + ",";
  json += "\"ignitionOffHeadlight\":" + String(config.ignitionOffHeadlight ? "true" : "false") + ",";
  json += "\"ignitionOffDRL\":" + String(config.ignitionOffDRL ? "true" : "false") + ",";
  json += "\"ignitionOffInterior\":" + String(config.ignitionOffInterior ? "true" : "false") + ",";
  json += "\"ignitionOffFoot\":" + String(config.ignitionOffFoot ? "true" : "false") + ",";
  
  json += "\"doorOpenEnable\":" + String(config.doorOpenEnable ? "true" : "false") + ",";
  json += "\"doorOpenHeadlight\":" + String(config.doorOpenHeadlight ? "true" : "false") + ",";
  json += "\"doorOpenDRL\":" + String(config.doorOpenDRL ? "true" : "false") + ",";
  json += "\"doorOpenInterior\":" + String(config.doorOpenInterior ? "true" : "false") + ",";
  json += "\"doorOpenFoot\":" + String(config.doorOpenFoot ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleSaveConfig() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    config.welcomeLightDuration = extractJsonValue(body, "welcomeLightDuration").toInt();
    config.goodbyeLightDuration = extractJsonValue(body, "goodbyeLightDuration").toInt();
    config.followMeHomeDuration = extractJsonValue(body, "followMeHomeDuration").toInt();
    config.doorOpenLightDuration = extractJsonValue(body, "doorOpenLightDuration").toInt();
    config.maxOnTime = extractJsonValue(body, "maxOnTime").toInt();
    
    config.unlockEnable = extractJsonValue(body, "unlockEnable") == "true";
    config.unlockHeadlight = extractJsonValue(body, "unlockHeadlight") == "true";
    config.unlockDRL = extractJsonValue(body, "unlockDRL") == "true";
    config.unlockInterior = extractJsonValue(body, "unlockInterior") == "true";
    config.unlockFoot = extractJsonValue(body, "unlockFoot") == "true";
    
    config.lockEnable = extractJsonValue(body, "lockEnable") == "true";
    config.lockHeadlight = extractJsonValue(body, "lockHeadlight") == "true";
    config.lockDRL = extractJsonValue(body, "lockDRL") == "true";
    config.lockInterior = extractJsonValue(body, "lockInterior") == "true";
    config.lockFoot = extractJsonValue(body, "lockFoot") == "true";
    
    config.ignitionOnEnable = extractJsonValue(body, "ignitionOnEnable") == "true";
    config.ignitionOnHeadlight = extractJsonValue(body, "ignitionOnHeadlight") == "true";
    config.ignitionOnDRL = extractJsonValue(body, "ignitionOnDRL") == "true";
    config.ignitionOnInterior = extractJsonValue(body, "ignitionOnInterior") == "true";
    config.ignitionOnFoot = extractJsonValue(body, "ignitionOnFoot") == "true";
    
    config.ignitionOffEnable = extractJsonValue(body, "ignitionOffEnable") == "true";
    config.ignitionOffHeadlight = extractJsonValue(body, "ignitionOffHeadlight") == "true";
    config.ignitionOffDRL = extractJsonValue(body, "ignitionOffDRL") == "true";
    config.ignitionOffInterior = extractJsonValue(body, "ignitionOffInterior") == "true";
    config.ignitionOffFoot = extractJsonValue(body, "ignitionOffFoot") == "true";
    
    config.doorOpenEnable = extractJsonValue(body, "doorOpenEnable") == "true";
    config.doorOpenHeadlight = extractJsonValue(body, "doorOpenHeadlight") == "true";
    config.doorOpenDRL = extractJsonValue(body, "doorOpenDRL") == "true";
    config.doorOpenInterior = extractJsonValue(body, "doorOpenInterior") == "true";
    config.doorOpenFoot = extractJsonValue(body, "doorOpenFoot") == "true";
    
    saveConfig();
    
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(400, "application/json", "{\"success\":false}");
  }
}

// ============================================
// FUNÇÕES DE CONTROLE DE LUZES
// ============================================

void updateOutputs() {
  // Resetar estados
  headlightOn = false;
  interiorOn = false;
  footOn = false;
  drlOn = false;
  
  // === WELCOME LIGHT (Destravar) ===
  if (welcomeLightActive && config.unlockEnable) {
    if (millis() - welcomeLightTimer < (config.welcomeLightDuration * 1000UL)) {
      if (config.unlockHeadlight) headlightOn = true;
      if (config.unlockDRL) drlOn = true;
      if (config.unlockInterior) interiorOn = true;
      if (config.unlockFoot) footOn = true;
    } else {
      welcomeLightActive = false;
      addLog("Welcome Light finalizado");
    }
  }
  
  // === FOLLOW ME HOME (Travar) ===
  if (followMeHomeActive && config.lockEnable) {
    if (millis() - followMeHomeTimer < (config.followMeHomeDuration * 1000UL)) {
      if (config.lockHeadlight) headlightOn = true;
      if (config.lockDRL) drlOn = true;
      if (config.lockInterior) interiorOn = true;
      if (config.lockFoot) footOn = true;
    } else {
      followMeHomeActive = false;
      addLog("Follow Me Home finalizado");
    }
  }
  
  // === GOODBYE LIGHT (Ignição desligada) ===
  if (goodbyeLightActive && config.ignitionOffEnable) {
    if (millis() - goodbyeLightTimer < (config.goodbyeLightDuration * 1000UL)) {
      if (config.ignitionOffHeadlight) headlightOn = true;
      if (config.ignitionOffDRL) drlOn = true;
      if (config.ignitionOffInterior) interiorOn = true;
      if (config.ignitionOffFoot) footOn = true;
    } else {
      goodbyeLightActive = false;
      addLog("Goodbye Light finalizado");
    }
  }
  
  // === LUZ AO ABRIR PORTA ===
  if (doorLightActive && config.doorOpenEnable) {
    if (millis() - doorLightTimer < (config.doorOpenLightDuration * 1000UL)) {
      if (config.doorOpenHeadlight) headlightOn = true;
      if (config.doorOpenDRL) drlOn = true;
      if (config.doorOpenInterior) interiorOn = true;
      if (config.doorOpenFoot) footOn = true;
    } else {
      doorLightActive = false;
    }
  }
  
  // === IGNIÇÃO LIGADA ===
  if (ignitionOn && config.ignitionOnEnable) {
    if (config.ignitionOnHeadlight) headlightOn = true;
    if (config.ignitionOnDRL) drlOn = true;
    if (config.ignitionOnInterior) interiorOn = true;
    if (config.ignitionOnFoot) footOn = true;
  }
  
  // === PORTA ABERTA (prioridade) ===
  if (doorOpen && config.doorOpenEnable) {
    if (config.doorOpenHeadlight) headlightOn = true;
    if (config.doorOpenDRL) drlOn = true;
    if (config.doorOpenInterior) interiorOn = true;
    if (config.doorOpenFoot) footOn = true;
  }
  
  // === PROTEÇÃO DE TIMEOUT ===
  // Só ativa timeout se houver luzes ligadas
  bool anyLightOn = (headlightOn || drlOn || interiorOn || footOn);
  
  if (anyLightOn) {
    // Se está tudo desligado, reseta o timer
    systemOnTimer = millis();
  } else if (!anyLightOn && !timeoutTriggered) {
    // Se nenhuma luz está ligada e timeout foi atingido, desligar tudo uma vez
    if (millis() - systemOnTimer > (config.maxOnTime * 1000UL)) {
      addLog("PROTEÇÃO: Timeout de bateria atingido!");
      timeoutTriggered = true;
    }
  }
  
  // Reset do timeout se voltou a ter luzes
  if (anyLightOn && timeoutTriggered) {
    timeoutTriggered = false;
    systemOnTimer = millis();
  }
  
  // Aplicar estados aos pinos
  digitalWrite(PIN_HEADLIGHT, headlightOn ? HIGH : LOW);
  digitalWrite(PIN_DRL, drlOn ? HIGH : LOW);
  digitalWrite(PIN_INTERIOR_LIGHT, interiorOn ? HIGH : LOW);
  digitalWrite(PIN_FOOT_LIGHT, footOn ? HIGH : LOW);
}


// ============================================
// FUNÇÃO DE DEBOUNCE
// ============================================

void debounceInputs() {
  // Implementar debounce movendo valores no buffer
  // O valor debounced é a maioria dos últimos DEBOUNCE_SAMPLES
  
  uint8_t rawLock = digitalRead(PIN_LOCK_SIGNAL);
  uint8_t rawUnlock = digitalRead(PIN_UNLOCK_SIGNAL);
  
  // Adicionar novos valores ao buffer
  lockDebounceBuffer[debounceIndex] = rawLock;
  unlockDebounceBuffer[debounceIndex] = rawUnlock;
  
  // Calcular maioria (voting)
  uint8_t lockCount = 0, unlockCount = 0;
  for (int i = 0; i < DEBOUNCE_SAMPLES; i++) {
    if (lockDebounceBuffer[i] == 0) lockCount++;
    if (unlockDebounceBuffer[i] == 0) unlockCount++;
  }
  
  // Atualizar sinais se maioria confirma (3 de 5 mínimo)
  lockSignal = (lockCount >= 3);
  unlockSignal = (unlockCount >= 3);
  
  debounceIndex = (debounceIndex + 1) % DEBOUNCE_SAMPLES;
}

void readInputs() {
  // Ler estados com debounce apenas para trava/destrava (sinais oscilantes)
  unsigned long currentTime = millis();
  
  if (currentTime - lastDebounceTime >= DEBOUNCE_DELAY) {
    debounceInputs();
    lastDebounceTime = currentTime;
  }
  
  // Ler porta e ignição diretamente (mais estáveis)
  doorOpen = !digitalRead(PIN_DOOR_OPEN);
  ignitionOn = !digitalRead(PIN_IGNITION);
}

void processEvents() {
  // === EVENTO: DESTRAVAR (Welcome Light) ===
  if (!unlockSignal && unlockSignalPrev && config.unlockEnable) {
    addLog("DESTRAVADO - Ativando Welcome Light");
    welcomeLightActive = true;
    welcomeLightTimer = millis();
    timeoutTriggered = false;
  }
  
  // === EVENTO: TRAVAR (Follow Me Home) ===
  if (!lockSignal && lockSignalPrev && config.lockEnable) {
    addLog("TRAVADO - Ativando Follow Me Home");
    followMeHomeActive = true;
    followMeHomeTimer = millis();
    timeoutTriggered = false;
  }
  
  // === EVENTO: DESLIGAR IGNIÇÃO (Goodbye Light) ===
  if (!ignitionOn && ignitionOnPrev && config.ignitionOffEnable) {
    addLog("IGNIÇÃO DESLIGADA - Ativando Goodbye Light");
    goodbyeLightActive = true;
    goodbyeLightTimer = millis();
    timeoutTriggered = false;
  }
  
  // === EVENTO: ABRIR PORTA ===
  if (doorOpen && !doorOpenPrev && config.doorOpenEnable) {
    addLog("PORTA ABERTA - Ativando luz interna");
    doorLightActive = true;
    doorLightTimer = millis();
    timeoutTriggered = false;
  }
  
  // Atualizar estados anteriores
  doorOpenPrev = doorOpen;
  lockSignalPrev = lockSignal;
  unlockSignalPrev = unlockSignal;
  ignitionOnPrev = ignitionOn;
}

// ============================================
// SETUP
// ============================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=================================");
  Serial.println("BCM ESP32 - Body Control Module");
  Serial.println("=================================\n");
  
  // Configurar pinos
  pinMode(PIN_DOOR_OPEN, INPUT_PULLUP);
  pinMode(PIN_LOCK_SIGNAL, INPUT_PULLUP);
  pinMode(PIN_UNLOCK_SIGNAL, INPUT_PULLUP);
  pinMode(PIN_IGNITION, INPUT_PULLUP);
  
  pinMode(PIN_HEADLIGHT, OUTPUT);
  pinMode(PIN_DRL, OUTPUT);
  pinMode(PIN_INTERIOR_LIGHT, OUTPUT);
  pinMode(PIN_FOOT_LIGHT, OUTPUT);
  
  // Desligar todas as saídas
  digitalWrite(PIN_HEADLIGHT, LOW);
  digitalWrite(PIN_DRL, LOW);
  digitalWrite(PIN_INTERIOR_LIGHT, LOW);
  digitalWrite(PIN_FOOT_LIGHT, LOW);
  
  // Carregar configuração
  loadConfig();
  
  // Iniciar WiFi AP
  Serial.println("Iniciando WiFi AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(IP);
  
  // Configurar rotas do servidor
  server.on("/", handleRoot);
  server.on("/getConfig", handleGetConfig);
  server.on("/getStatus", handleGetStatus);
  server.on("/saveConfig", HTTP_POST, handleSaveConfig);
  
  server.begin();
  Serial.println("Servidor Web iniciado!");
  Serial.println("\nSistema pronto!");
  Serial.println("Conecte-se ao WiFi: " + String(AP_SSID));
  Serial.println("Acesse: http://" + IP.toString() + "\n");
  
  // Inicializar estados
  readInputs();
  doorOpenPrev = doorOpen;
  lockSignalPrev = lockSignal;
  unlockSignalPrev = unlockSignal;
  ignitionOnPrev = ignitionOn;
  systemOnTimer = millis();
  
  // Log inicial
  addLog("Sistema iniciado com sucesso!");
}

// ============================================
// LOOP
// ============================================

void loop() {
  server.handleClient();
  
  readInputs();
  processEvents();
  updateOutputs();
  
  delay(50); // 50ms loop
}