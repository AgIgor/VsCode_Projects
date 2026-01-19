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
  uint16_t welcomeLightDuration;     // Duração Welcome Light
  uint16_t goodbyeLightDuration;     // Duração Goodbye Light
  uint16_t followMeHomeDuration;     // Duração Follow Me Home
  uint16_t doorOpenLightDuration;    // Duração luz ao abrir porta
  uint16_t maxOnTime;                // Tempo máximo ligado (proteção bateria)
  
  // Habilitações gerais
  bool enableWelcomeLight;           // Ativar Welcome Light ao destravar
  bool enableGoodbyeLight;           // Ativar Goodbye Light ao desligar ignição
  bool enableFollowMeHome;           // Ativar Follow Me Home ao travar
  bool enableHeadlightOnIgnition;    // Ligar farois ao ligar ignição
  bool enableInteriorOnDoorOpen;     // Ligar luz interna ao abrir porta
  bool enableDRL;                    // Ativar DRL
  
  // Welcome Light - Saídas ativadas
  bool welcomeHeadlight;
  bool welcomeDRL;
  bool welcomeInterior;
  bool welcomeFoot;
  
  // Goodbye Light - Saídas ativadas
  bool goodbyeHeadlight;
  bool goodbyeDRL;
  bool goodbyeInterior;
  bool goodbyeFoot;
  
  // Follow Me Home - Saídas ativadas
  bool followMeHeadlight;
  bool followMeDRL;
  bool followMeInterior;
  bool followMeFoot;
  
  // Door Open - Saídas ativadas
  bool doorHeadlight;
  bool doorDRL;
  bool doorInterior;
  bool doorFoot;
  
  // Ignition On - Saídas ativadas
  bool ignitionHeadlight;
  bool ignitionDRL;
  bool ignitionInterior;
  bool ignitionFoot;
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
                <h2>🔧 Configurações de Modos</h2>
                <div class='checkbox-group'>
                    <input type='checkbox' id='enableWelcome' checked>
                    <label for='enableWelcome'>Ativar Welcome Light ao Destravar</label>
                </div>
                <div class='checkbox-group'>
                    <input type='checkbox' id='enableGoodbye' checked>
                    <label for='enableGoodbye'>Ativar Goodbye Light ao Desligar Ignição</label>
                </div>
                <div class='checkbox-group'>
                    <input type='checkbox' id='enableFollowMe' checked>
                    <label for='enableFollowMe'>Ativar Follow Me Home ao Travar</label>
                </div>
                <div class='checkbox-group'>
                    <input type='checkbox' id='enableHeadlightIgnition' checked>
                    <label for='enableHeadlightIgnition'>Ligar Farois ao Ligar Ignição</label>
                </div>
                <div class='checkbox-group'>
                    <input type='checkbox' id='enableInteriorDoor' checked>
                    <label for='enableInteriorDoor'>Ligar Luz Interna ao Abrir Porta</label>
                </div>
                <div class='checkbox-group'>
                    <input type='checkbox' id='enableDRL' checked>
                    <label for='enableDRL'>Ativar DRL (Luz Diurna)</label>
                </div>
            </div>

            <div class='section'>
                <h2>⚙️ Saídas por Modo</h2>
                
                <div style='margin-bottom: 25px; padding: 15px; background: white; border-radius: 8px; border-left: 4px solid #28a745;'>
                    <h3 style='color: #28a745; margin-bottom: 10px;'>Welcome Light (Ao Destravar)</h3>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='welcomeHeadlight' checked> <label for='welcomeHeadlight'>💡 Farois</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='welcomeDRL' checked> <label for='welcomeDRL'>☀️ DRL</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='welcomeInterior' checked> <label for='welcomeInterior'>🏠 Luz Interna</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0;'>
                        <input type='checkbox' id='welcomeFoot' checked> <label for='welcomeFoot'>👣 Luz Pés</label>
                    </div>
                </div>

                <div style='margin-bottom: 25px; padding: 15px; background: white; border-radius: 8px; border-left: 4px solid #ffc107;'>
                    <h3 style='color: #ffc107; margin-bottom: 10px;'>Goodbye Light (Ao Desligar Ignição)</h3>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='goodbyeHeadlight' checked> <label for='goodbyeHeadlight'>💡 Farois</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='goodbyeDRL'> <label for='goodbyeDRL'>☀️ DRL</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='goodbyeInterior'> <label for='goodbyeInterior'>🏠 Luz Interna</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0;'>
                        <input type='checkbox' id='goodbyeFoot'> <label for='goodbyeFoot'>👣 Luz Pés</label>
                    </div>
                </div>

                <div style='margin-bottom: 25px; padding: 15px; background: white; border-radius: 8px; border-left: 4px solid #17a2b8;'>
                    <h3 style='color: #17a2b8; margin-bottom: 10px;'>Follow Me Home (Ao Travar)</h3>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='followMeHeadlight' checked> <label for='followMeHeadlight'>💡 Farois</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='followMeDRL' checked> <label for='followMeDRL'>☀️ DRL</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='followMeInterior' checked> <label for='followMeInterior'>🏠 Luz Interna</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0;'>
                        <input type='checkbox' id='followMeFoot' checked> <label for='followMeFoot'>👣 Luz Pés</label>
                    </div>
                </div>

                <div style='margin-bottom: 25px; padding: 15px; background: white; border-radius: 8px; border-left: 4px solid #6c757d;'>
                    <h3 style='color: #6c757d; margin-bottom: 10px;'>Luz ao Abrir Porta</h3>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='doorHeadlight'> <label for='doorHeadlight'>💡 Farois</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='doorDRL'> <label for='doorDRL'>☀️ DRL</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='doorInterior' checked> <label for='doorInterior'>🏠 Luz Interna</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0;'>
                        <input type='checkbox' id='doorFoot' checked> <label for='doorFoot'>👣 Luz Pés</label>
                    </div>
                </div>

                <div style='padding: 15px; background: white; border-radius: 8px; border-left: 4px solid #20c997;'>
                    <h3 style='color: #20c997; margin-bottom: 10px;'>Ignição Ligada</h3>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='ignitionHeadlight' checked> <label for='ignitionHeadlight'>💡 Farois</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='ignitionDRL' checked> <label for='ignitionDRL'>☀️ DRL</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0; margin-bottom: 8px;'>
                        <input type='checkbox' id='ignitionInterior'> <label for='ignitionInterior'>🏠 Luz Interna</label>
                    </div>
                    <div class='checkbox-group' style='background: transparent; padding: 0;'>
                        <input type='checkbox' id='ignitionFoot'> <label for='ignitionFoot'>👣 Luz Pés</label>
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
                    
                    document.getElementById('enableWelcome').checked = data.enableWelcomeLight;
                    document.getElementById('enableGoodbye').checked = data.enableGoodbyeLight;
                    document.getElementById('enableFollowMe').checked = data.enableFollowMeHome;
                    document.getElementById('enableHeadlightIgnition').checked = data.enableHeadlightOnIgnition;
                    document.getElementById('enableInteriorDoor').checked = data.enableInteriorOnDoorOpen;
                    document.getElementById('enableDRL').checked = data.enableDRL;
                    
                    // Welcome Light
                    document.getElementById('welcomeHeadlight').checked = data.welcomeHeadlight;
                    document.getElementById('welcomeDRL').checked = data.welcomeDRL;
                    document.getElementById('welcomeInterior').checked = data.welcomeInterior;
                    document.getElementById('welcomeFoot').checked = data.welcomeFoot;
                    
                    // Goodbye Light
                    document.getElementById('goodbyeHeadlight').checked = data.goodbyeHeadlight;
                    document.getElementById('goodbyeDRL').checked = data.goodbyeDRL;
                    document.getElementById('goodbyeInterior').checked = data.goodbyeInterior;
                    document.getElementById('goodbyeFoot').checked = data.goodbyeFoot;
                    
                    // Follow Me Home
                    document.getElementById('followMeHeadlight').checked = data.followMeHeadlight;
                    document.getElementById('followMeDRL').checked = data.followMeDRL;
                    document.getElementById('followMeInterior').checked = data.followMeInterior;
                    document.getElementById('followMeFoot').checked = data.followMeFoot;
                    
                    // Door Open
                    document.getElementById('doorHeadlight').checked = data.doorHeadlight;
                    document.getElementById('doorDRL').checked = data.doorDRL;
                    document.getElementById('doorInterior').checked = data.doorInterior;
                    document.getElementById('doorFoot').checked = data.doorFoot;
                    
                    // Ignition On
                    document.getElementById('ignitionHeadlight').checked = data.ignitionHeadlight;
                    document.getElementById('ignitionDRL').checked = data.ignitionDRL;
                    document.getElementById('ignitionInterior').checked = data.ignitionInterior;
                    document.getElementById('ignitionFoot').checked = data.ignitionFoot;
                    
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
                
                enableWelcomeLight: document.getElementById('enableWelcome').checked,
                enableGoodbyeLight: document.getElementById('enableGoodbye').checked,
                enableFollowMeHome: document.getElementById('enableFollowMe').checked,
                enableHeadlightOnIgnition: document.getElementById('enableHeadlightIgnition').checked,
                enableInteriorOnDoorOpen: document.getElementById('enableInteriorDoor').checked,
                enableDRL: document.getElementById('enableDRL').checked,
                
                // Welcome Light
                welcomeHeadlight: document.getElementById('welcomeHeadlight').checked,
                welcomeDRL: document.getElementById('welcomeDRL').checked,
                welcomeInterior: document.getElementById('welcomeInterior').checked,
                welcomeFoot: document.getElementById('welcomeFoot').checked,
                
                // Goodbye Light
                goodbyeHeadlight: document.getElementById('goodbyeHeadlight').checked,
                goodbyeDRL: document.getElementById('goodbyeDRL').checked,
                goodbyeInterior: document.getElementById('goodbyeInterior').checked,
                goodbyeFoot: document.getElementById('goodbyeFoot').checked,
                
                // Follow Me Home
                followMeHeadlight: document.getElementById('followMeHeadlight').checked,
                followMeDRL: document.getElementById('followMeDRL').checked,
                followMeInterior: document.getElementById('followMeInterior').checked,
                followMeFoot: document.getElementById('followMeFoot').checked,
                
                // Door Open
                doorHeadlight: document.getElementById('doorHeadlight').checked,
                doorDRL: document.getElementById('doorDRL').checked,
                doorInterior: document.getElementById('doorInterior').checked,
                doorFoot: document.getElementById('doorFoot').checked,
                
                // Ignition On
                ignitionHeadlight: document.getElementById('ignitionHeadlight').checked,
                ignitionDRL: document.getElementById('ignitionDRL').checked,
                ignitionInterior: document.getElementById('ignitionInterior').checked,
                ignitionFoot: document.getElementById('ignitionFoot').checked
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
  config.maxOnTime = 600; // 10 minutos
  
  config.enableWelcomeLight = true;
  config.enableGoodbyeLight = true;
  config.enableFollowMeHome = true;
  config.enableHeadlightOnIgnition = true;
  config.enableInteriorOnDoorOpen = true;
  config.enableDRL = true;
  
  // Welcome Light padrão: todas as saídas
  config.welcomeHeadlight = true;
  config.welcomeDRL = true;
  config.welcomeInterior = true;
  config.welcomeFoot = true;
  
  // Goodbye Light padrão: apenas farol
  config.goodbyeHeadlight = true;
  config.goodbyeDRL = false;
  config.goodbyeInterior = false;
  config.goodbyeFoot = false;
  
  // Follow Me Home padrão: todas as saídas
  config.followMeHeadlight = true;
  config.followMeDRL = true;
  config.followMeInterior = true;
  config.followMeFoot = true;
  
  // Door Open padrão: interior e pés
  config.doorHeadlight = false;
  config.doorDRL = false;
  config.doorInterior = true;
  config.doorFoot = true;
  
  // Ignition On padrão: farol e DRL
  config.ignitionHeadlight = true;
  config.ignitionDRL = true;
  config.ignitionInterior = false;
  config.ignitionFoot = false;
}

void loadConfig() {
  preferences.begin("bcm-config", false);
  
  config.welcomeLightDuration = preferences.getUShort("welcome", 30);
  config.goodbyeLightDuration = preferences.getUShort("goodbye", 30);
  config.followMeHomeDuration = preferences.getUShort("followme", 60);
  config.doorOpenLightDuration = preferences.getUShort("door", 20);
  config.maxOnTime = preferences.getUShort("maxtime", 600);
  
  config.enableWelcomeLight = preferences.getBool("en_welcome", true);
  config.enableGoodbyeLight = preferences.getBool("en_goodbye", true);
  config.enableFollowMeHome = preferences.getBool("en_followme", true);
  config.enableHeadlightOnIgnition = preferences.getBool("en_headign", true);
  config.enableInteriorOnDoorOpen = preferences.getBool("en_intdoor", true);
  config.enableDRL = preferences.getBool("en_drl", true);
  
  // Welcome Light
  config.welcomeHeadlight = preferences.getBool("wel_head", true);
  config.welcomeDRL = preferences.getBool("wel_drl", true);
  config.welcomeInterior = preferences.getBool("wel_int", true);
  config.welcomeFoot = preferences.getBool("wel_foot", true);
  
  // Goodbye Light
  config.goodbyeHeadlight = preferences.getBool("gdb_head", true);
  config.goodbyeDRL = preferences.getBool("gdb_drl", false);
  config.goodbyeInterior = preferences.getBool("gdb_int", false);
  config.goodbyeFoot = preferences.getBool("gdb_foot", false);
  
  // Follow Me Home
  config.followMeHeadlight = preferences.getBool("fmh_head", true);
  config.followMeDRL = preferences.getBool("fmh_drl", true);
  config.followMeInterior = preferences.getBool("fmh_int", true);
  config.followMeFoot = preferences.getBool("fmh_foot", true);
  
  // Door Open
  config.doorHeadlight = preferences.getBool("dor_head", false);
  config.doorDRL = preferences.getBool("dor_drl", false);
  config.doorInterior = preferences.getBool("dor_int", true);
  config.doorFoot = preferences.getBool("dor_foot", true);
  
  // Ignition On
  config.ignitionHeadlight = preferences.getBool("ign_head", true);
  config.ignitionDRL = preferences.getBool("ign_drl", true);
  config.ignitionInterior = preferences.getBool("ign_int", false);
  config.ignitionFoot = preferences.getBool("ign_foot", false);
  
  preferences.end();
  
  Serial.println("Configuração carregada:");
  Serial.printf("Welcome: %ds, Goodbye: %ds, Follow Me: %ds\n", 
                config.welcomeLightDuration, config.goodbyeLightDuration, config.followMeHomeDuration);
}

void saveConfig() {
  preferences.begin("bcm-config", false);
  
  preferences.putUShort("welcome", config.welcomeLightDuration);
  preferences.putUShort("goodbye", config.goodbyeLightDuration);
  preferences.putUShort("followme", config.followMeHomeDuration);
  preferences.putUShort("door", config.doorOpenLightDuration);
  preferences.putUShort("maxtime", config.maxOnTime);
  
  preferences.putBool("en_welcome", config.enableWelcomeLight);
  preferences.putBool("en_goodbye", config.enableGoodbyeLight);
  preferences.putBool("en_followme", config.enableFollowMeHome);
  preferences.putBool("en_headign", config.enableHeadlightOnIgnition);
  preferences.putBool("en_intdoor", config.enableInteriorOnDoorOpen);
  preferences.putBool("en_drl", config.enableDRL);
  
  // Welcome Light
  preferences.putBool("wel_head", config.welcomeHeadlight);
  preferences.putBool("wel_drl", config.welcomeDRL);
  preferences.putBool("wel_int", config.welcomeInterior);
  preferences.putBool("wel_foot", config.welcomeFoot);
  
  // Goodbye Light
  preferences.putBool("gdb_head", config.goodbyeHeadlight);
  preferences.putBool("gdb_drl", config.goodbyeDRL);
  preferences.putBool("gdb_int", config.goodbyeInterior);
  preferences.putBool("gdb_foot", config.goodbyeFoot);
  
  // Follow Me Home
  preferences.putBool("fmh_head", config.followMeHeadlight);
  preferences.putBool("fmh_drl", config.followMeDRL);
  preferences.putBool("fmh_int", config.followMeInterior);
  preferences.putBool("fmh_foot", config.followMeFoot);
  
  // Door Open
  preferences.putBool("dor_head", config.doorHeadlight);
  preferences.putBool("dor_drl", config.doorDRL);
  preferences.putBool("dor_int", config.doorInterior);
  preferences.putBool("dor_foot", config.doorFoot);
  
  // Ignition On
  preferences.putBool("ign_head", config.ignitionHeadlight);
  preferences.putBool("ign_drl", config.ignitionDRL);
  preferences.putBool("ign_int", config.ignitionInterior);
  preferences.putBool("ign_foot", config.ignitionFoot);
  
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
  json += "\"enableWelcomeLight\":" + String(config.enableWelcomeLight ? "true" : "false") + ",";
  json += "\"enableGoodbyeLight\":" + String(config.enableGoodbyeLight ? "true" : "false") + ",";
  json += "\"enableFollowMeHome\":" + String(config.enableFollowMeHome ? "true" : "false") + ",";
  json += "\"enableHeadlightOnIgnition\":" + String(config.enableHeadlightOnIgnition ? "true" : "false") + ",";
  json += "\"enableInteriorOnDoorOpen\":" + String(config.enableInteriorOnDoorOpen ? "true" : "false") + ",";
  json += "\"enableDRL\":" + String(config.enableDRL ? "true" : "false") + ",";
  
  // Welcome Light
  json += "\"welcomeHeadlight\":" + String(config.welcomeHeadlight ? "true" : "false") + ",";
  json += "\"welcomeDRL\":" + String(config.welcomeDRL ? "true" : "false") + ",";
  json += "\"welcomeInterior\":" + String(config.welcomeInterior ? "true" : "false") + ",";
  json += "\"welcomeFoot\":" + String(config.welcomeFoot ? "true" : "false") + ",";
  
  // Goodbye Light
  json += "\"goodbyeHeadlight\":" + String(config.goodbyeHeadlight ? "true" : "false") + ",";
  json += "\"goodbyeDRL\":" + String(config.goodbyeDRL ? "true" : "false") + ",";
  json += "\"goodbyeInterior\":" + String(config.goodbyeInterior ? "true" : "false") + ",";
  json += "\"goodbyeFoot\":" + String(config.goodbyeFoot ? "true" : "false") + ",";
  
  // Follow Me Home
  json += "\"followMeHeadlight\":" + String(config.followMeHeadlight ? "true" : "false") + ",";
  json += "\"followMeDRL\":" + String(config.followMeDRL ? "true" : "false") + ",";
  json += "\"followMeInterior\":" + String(config.followMeInterior ? "true" : "false") + ",";
  json += "\"followMeFoot\":" + String(config.followMeFoot ? "true" : "false") + ",";
  
  // Door Open
  json += "\"doorHeadlight\":" + String(config.doorHeadlight ? "true" : "false") + ",";
  json += "\"doorDRL\":" + String(config.doorDRL ? "true" : "false") + ",";
  json += "\"doorInterior\":" + String(config.doorInterior ? "true" : "false") + ",";
  json += "\"doorFoot\":" + String(config.doorFoot ? "true" : "false") + ",";
  
  // Ignition On
  json += "\"ignitionHeadlight\":" + String(config.ignitionHeadlight ? "true" : "false") + ",";
  json += "\"ignitionDRL\":" + String(config.ignitionDRL ? "true" : "false") + ",";
  json += "\"ignitionInterior\":" + String(config.ignitionInterior ? "true" : "false") + ",";
  json += "\"ignitionFoot\":" + String(config.ignitionFoot ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleSaveConfig() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    // Parse JSON manualmente (simples)
    config.welcomeLightDuration = extractJsonValue(body, "welcomeLightDuration").toInt();
    config.goodbyeLightDuration = extractJsonValue(body, "goodbyeLightDuration").toInt();
    config.followMeHomeDuration = extractJsonValue(body, "followMeHomeDuration").toInt();
    config.doorOpenLightDuration = extractJsonValue(body, "doorOpenLightDuration").toInt();
    config.maxOnTime = extractJsonValue(body, "maxOnTime").toInt();
    
    config.enableWelcomeLight = extractJsonValue(body, "enableWelcomeLight") == "true";
    config.enableGoodbyeLight = extractJsonValue(body, "enableGoodbyeLight") == "true";
    config.enableFollowMeHome = extractJsonValue(body, "enableFollowMeHome") == "true";
    config.enableHeadlightOnIgnition = extractJsonValue(body, "enableHeadlightOnIgnition") == "true";
    config.enableInteriorOnDoorOpen = extractJsonValue(body, "enableInteriorOnDoorOpen") == "true";
    config.enableDRL = extractJsonValue(body, "enableDRL") == "true";
    
    // Welcome Light
    config.welcomeHeadlight = extractJsonValue(body, "welcomeHeadlight") == "true";
    config.welcomeDRL = extractJsonValue(body, "welcomeDRL") == "true";
    config.welcomeInterior = extractJsonValue(body, "welcomeInterior") == "true";
    config.welcomeFoot = extractJsonValue(body, "welcomeFoot") == "true";
    
    // Goodbye Light
    config.goodbyeHeadlight = extractJsonValue(body, "goodbyeHeadlight") == "true";
    config.goodbyeDRL = extractJsonValue(body, "goodbyeDRL") == "true";
    config.goodbyeInterior = extractJsonValue(body, "goodbyeInterior") == "true";
    config.goodbyeFoot = extractJsonValue(body, "goodbyeFoot") == "true";
    
    // Follow Me Home
    config.followMeHeadlight = extractJsonValue(body, "followMeHeadlight") == "true";
    config.followMeDRL = extractJsonValue(body, "followMeDRL") == "true";
    config.followMeInterior = extractJsonValue(body, "followMeInterior") == "true";
    config.followMeFoot = extractJsonValue(body, "followMeFoot") == "true";
    
    // Door Open
    config.doorHeadlight = extractJsonValue(body, "doorHeadlight") == "true";
    config.doorDRL = extractJsonValue(body, "doorDRL") == "true";
    config.doorInterior = extractJsonValue(body, "doorInterior") == "true";
    config.doorFoot = extractJsonValue(body, "doorFoot") == "true";
    
    // Ignition On
    config.ignitionHeadlight = extractJsonValue(body, "ignitionHeadlight") == "true";
    config.ignitionDRL = extractJsonValue(body, "ignitionDRL") == "true";
    config.ignitionInterior = extractJsonValue(body, "ignitionInterior") == "true";
    config.ignitionFoot = extractJsonValue(body, "ignitionFoot") == "true";
    
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
  // Sistema de proteção - desligar tudo se exceder tempo máximo
  if (millis() - systemOnTimer > (config.maxOnTime * 1000UL)) {
    digitalWrite(PIN_HEADLIGHT, LOW);
    digitalWrite(PIN_DRL, LOW);
    digitalWrite(PIN_INTERIOR_LIGHT, LOW);
    digitalWrite(PIN_FOOT_LIGHT, LOW);
    addLog("PROTEÇÃO: Tempo máximo atingido!");
    
    // Reset timers
    welcomeLightActive = false;
    goodbyeLightActive = false;
    followMeHomeActive = false;
    doorLightActive = false;
    headlightOn = false;
    drlOn = false;
    interiorOn = false;
    footOn = false;
    return;
  }
  
  // Resetar estados
  headlightOn = false;
  interiorOn = false;
  footOn = false;
  drlOn = false;
  
  // === WELCOME LIGHT ===
  if (welcomeLightActive) {
    if (millis() - welcomeLightTimer < (config.welcomeLightDuration * 1000UL)) {
      if (config.welcomeHeadlight) headlightOn = true;
      if (config.welcomeDRL) drlOn = true;
      if (config.welcomeInterior) interiorOn = true;
      if (config.welcomeFoot) footOn = true;
    } else {
      welcomeLightActive = false;
      addLog("Welcome Light finalizado");
    }
  }
  
  // === GOODBYE LIGHT ===
  if (goodbyeLightActive) {
    if (millis() - goodbyeLightTimer < (config.goodbyeLightDuration * 1000UL)) {
      if (config.goodbyeHeadlight) headlightOn = true;
      if (config.goodbyeDRL) drlOn = true;
      if (config.goodbyeInterior) interiorOn = true;
      if (config.goodbyeFoot) footOn = true;
    } else {
      goodbyeLightActive = false;
      addLog("Goodbye Light finalizado");
    }
  }
  
  // === FOLLOW ME HOME ===
  if (followMeHomeActive) {
    if (millis() - followMeHomeTimer < (config.followMeHomeDuration * 1000UL)) {
      if (config.followMeHeadlight) headlightOn = true;
      if (config.followMeDRL) drlOn = true;
      if (config.followMeInterior) interiorOn = true;
      if (config.followMeFoot) footOn = true;
    } else {
      followMeHomeActive = false;
      addLog("Follow Me Home finalizado");
    }
  }
  
  // === LUZ AO ABRIR PORTA ===
  if (doorLightActive) {
    if (millis() - doorLightTimer < (config.doorOpenLightDuration * 1000UL)) {
      if (config.doorHeadlight) headlightOn = true;
      if (config.doorDRL) drlOn = true;
      if (config.doorInterior) interiorOn = true;
      if (config.doorFoot) footOn = true;
    } else {
      doorLightActive = false;
    }
  }
  
  // === IGNIÇÃO LIGADA ===
  if (ignitionOn) {
    if (config.enableHeadlightOnIgnition && config.ignitionHeadlight) {
      headlightOn = true;
    }
    if (config.enableDRL && config.ignitionDRL) {
      drlOn = true;
    }
    if (config.ignitionInterior) interiorOn = true;
    if (config.ignitionFoot) footOn = true;
  }
  
  // === PORTA ABERTA (prioridade) ===
  if (doorOpen && config.enableInteriorOnDoorOpen) {
    interiorOn = true;
    footOn = true;
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
  // Detecta borda de subida do sinal de destrava (LOW -> HIGH, pulso terminou)
  if (!unlockSignal && unlockSignalPrev && config.enableWelcomeLight) {
    addLog("DESTRAVADO - Ativando Welcome Light");
    welcomeLightActive = true;
    welcomeLightTimer = millis();
    systemOnTimer = millis(); // Reset proteção
  }
  
  // === EVENTO: TRAVAR (Follow Me Home) ===
  // Detecta borda de subida do sinal de trava (LOW -> HIGH, pulso terminou)
  if (!lockSignal && lockSignalPrev && config.enableFollowMeHome) {
    addLog("TRAVADO - Ativando Follow Me Home");
    followMeHomeActive = true;
    followMeHomeTimer = millis();
    systemOnTimer = millis(); // Reset proteção
  }
  
  // === EVENTO: DESLIGAR IGNIÇÃO (Goodbye Light) ===
  if (!ignitionOn && ignitionOnPrev && config.enableGoodbyeLight) {
    addLog("IGNIÇÃO DESLIGADA - Ativando Goodbye Light");
    goodbyeLightActive = true;
    goodbyeLightTimer = millis();
    systemOnTimer = millis(); // Reset proteção
  }
  
  // === EVENTO: ABRIR PORTA ===
  if (doorOpen && !doorOpenPrev && config.enableInteriorOnDoorOpen) {
    addLog("PORTA ABERTA - Ativando luz interna");
    doorLightActive = true;
    doorLightTimer = millis();
    systemOnTimer = millis(); // Reset proteção
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