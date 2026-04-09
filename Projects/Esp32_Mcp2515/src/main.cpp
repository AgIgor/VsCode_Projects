#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <mcp_can.h>
#include <cctype>
#include <cstring>
#include <cstdlib>

// ==========================================================
// ESP32 DevKit V1 + MCP2515 (SPI)
// SCK=18 | MISO=19 | MOSI=23 | CS=5 | INT=4
// ==========================================================
constexpr uint8_t CAN_CS_PIN = 5;
constexpr uint8_t CAN_INT_PIN = 4;
constexpr uint8_t CAN_SCK_PIN = 18;
constexpr uint8_t CAN_MISO_PIN = 19;
constexpr uint8_t CAN_MOSI_PIN = 23;

constexpr uint32_t SERIAL_BAUD = 230400;
constexpr uint8_t MCP2515_CLOCK = MCP_8MHZ;      // troque para MCP_16MHZ se seu módulo usar cristal de 16 MHz
constexpr uint8_t CAN_BUS_SPEED = CAN_500KBPS;   // troque para CAN_250KBPS se a rede do carro for 250 kbps
constexpr char WIFI_STA_SSID[] = "";             // opcional: preencha se quiser conectar no Wi-Fi da casa/oficina
constexpr char WIFI_STA_PASS[] = "";
constexpr char WIFI_AP_SSID[] = "ESP32-CAN-Hacker";
constexpr char WIFI_AP_PASS[] = "12345678";

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000;
constexpr uint32_t OBD_REQUEST_INTERVAL_MS = 350;
constexpr uint32_t DASHBOARD_INTERVAL_MS = 1000;
constexpr uint32_t RAW_REPRINT_MS = 500;
constexpr size_t MAX_TRACKED_IDS = 32;
constexpr size_t MAX_LOGGED_FRAMES = 180;

MCP_CAN CAN(CAN_CS_PIN);
WebServer server(80);

struct LiveData {
  bool rpmValid = false;
  bool speedValid = false;
  bool coolantValid = false;
  bool throttleValid = false;
  bool loadValid = false;
  bool fuelValid = false;
  bool intakeValid = false;

  float rpm = 0.0f;
  uint8_t speed = 0;
  int coolant = 0;
  float throttle = 0.0f;
  float engineLoad = 0.0f;
  float fuelLevel = 0.0f;
  int intakeTemp = 0;

  uint32_t lastResponseMs = 0;
} liveData;

struct FrameTracker {
  bool used = false;
  uint32_t id = 0;
  uint8_t len = 0;
  uint8_t data[8] = {0};
  uint32_t lastPrintMs = 0;
};

struct FrameLogEntry {
  uint32_t seq = 0;
  uint32_t timestampMs = 0;
  uint32_t id = 0;
  bool extended = false;
  bool rtr = false;
  uint8_t len = 0;
  uint8_t data[8] = {0};
};

FrameTracker trackedFrames[MAX_TRACKED_IDS];
FrameLogEntry frameLog[MAX_LOGGED_FRAMES];

bool rawOutputEnabled = true;
bool printOnlyChangedFrames = true;
bool obdPollingEnabled = true;
uint32_t rxFrameCount = 0;
uint32_t lastObdRequestMs = 0;
uint32_t lastObdErrorMs = 0;
uint32_t lastDashboardMs = 0;
uint32_t frameSequence = 0;
size_t nextPidIndex = 0;
size_t frameLogHead = 0;
size_t frameLogCount = 0;

const uint8_t obdPidList[] = {
  0x0C, // RPM
  0x0D, // velocidade
  0x05, // temperatura do motor
  0x11, // throttle
  0x04, // carga do motor
  0x2F, // combustível
  0x0F  // temperatura do ar de admissão
};

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 CAN Hacker</title>
  <style>
    :root{--bg:#0b1020;--card:#131a2e;--line:#24304d;--text:#e8f1ff;--accent:#39d98a;--warn:#ffb020}
    *{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:Arial,sans-serif}
    .wrap{max-width:1200px;margin:auto;padding:16px}.title{display:flex;justify-content:space-between;gap:10px;flex-wrap:wrap;align-items:center}
    .muted{color:#9eb2d2}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(155px,1fr));gap:10px;margin:14px 0}
    .card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px}.card strong{display:block;font-size:1.3rem;margin-top:6px;color:var(--accent)}
    .panel{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px;margin:10px 0}
    button{background:#1c2742;color:var(--text);border:1px solid #35508a;border-radius:10px;padding:9px 12px;cursor:pointer}
    button:hover{background:#24345d}input{background:#09101d;color:var(--text);border:1px solid #30476d;border-radius:8px;padding:9px;width:100%}
    .controls,.send{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px}.send{align-items:end}
    table{width:100%;border-collapse:collapse;font-size:.92rem}th,td{padding:8px;border-bottom:1px solid #1f2c47;text-align:left}thead{position:sticky;top:0;background:#0e1730}
    .table-wrap{max-height:420px;overflow:auto}.ok{color:var(--accent)}.warn{color:var(--warn)}code{color:#8fe3ff}
  </style>
</head>
<body>
  <div class="wrap">
    <div class="title">
      <div>
        <h1>ESP32 CAN Hacker Web</h1>
        <div class="muted">Sniffer CAN/OBD-II estilo CAN Hacker via navegador</div>
      </div>
      <div class="muted" id="wifiInfo">Carregando...</div>
    </div>

    <div class="grid">
      <div class="card"><span>Frames RX</span><strong id="rxCount">0</strong></div>
      <div class="card"><span>RPM</span><strong id="rpm">--</strong></div>
      <div class="card"><span>Velocidade</span><strong id="speed">--</strong></div>
      <div class="card"><span>Temp. motor</span><strong id="coolant">--</strong></div>
      <div class="card"><span>Throttle</span><strong id="throttle">--</strong></div>
      <div class="card"><span>Combustível</span><strong id="fuel">--</strong></div>
    </div>

    <div class="panel">
      <div class="controls">
        <button onclick="toggleFlag('raw')">Alternar RAW serial</button>
        <button onclick="toggleFlag('changes')">Somente mudanças</button>
        <button onclick="toggleFlag('obd')">OBD polling</button>
        <button onclick="clearFrames()">Limpar buffer</button>
        <button id="pauseBtn" onclick="togglePause()">Pausar captura</button>
        <input id="idFilter" placeholder="Filtrar ID (ex: 7E8)">
      </div>
      <p class="muted" id="statusLine">Lendo status...</p>
    </div>

    <div class="panel">
      <h3>Enviar frame CAN manual</h3>
      <form class="send" onsubmit="sendFrame(event)">
        <div><label>ID hex</label><input id="txId" value="7DF" maxlength="8"></div>
        <div><label>Dados hex</label><input id="txData" value="02 01 0C 00 00 00 00 00"></div>
        <div><label>Frame</label><input id="txType" value="0 = padrão / 1 = estendido"></div>
        <div><button type="submit">Enviar</button></div>
      </form>
      <p class="muted">Aceita formatos como <code>11 22 33 AA</code> ou <code>112233AA</code>.</p>
    </div>

    <div class="panel table-wrap">
      <table>
        <thead><tr><th>#</th><th>Tempo</th><th>ID</th><th>Tipo</th><th>DLC</th><th>Dados</th></tr></thead>
        <tbody id="frames"></tbody>
      </table>
    </div>
  </div>

<script>
let lastSeq = 0;
let paused = false;

function val(valid, value, suffix='') { return valid ? `${value}${suffix}` : '--'; }

function applyFilter() {
  const filter = document.getElementById('idFilter').value.trim().toUpperCase();
  document.querySelectorAll('#frames tr').forEach((row) => {
    row.style.display = !filter || row.dataset.id.includes(filter) ? '' : 'none';
  });
}

function addFrameRow(frame) {
  const tbody = document.getElementById('frames');
  const row = document.createElement('tr');
  row.dataset.id = frame.id.toUpperCase();
  row.innerHTML = `<td>${frame.seq}</td><td>${frame.ts} ms</td><td>${frame.id}</td><td>${frame.type}</td><td>${frame.dlc}</td><td><code>${frame.data || '--'}</code></td>`;
  tbody.prepend(row);
  while (tbody.children.length > 250) tbody.removeChild(tbody.lastChild);
}

async function refreshStatus() {
  const resp = await fetch('/api/status');
  const s = await resp.json();
  document.getElementById('rxCount').textContent = s.rxCount;
  document.getElementById('rpm').textContent = val(s.rpmValid, s.rpm.toFixed(0), ' rpm');
  document.getElementById('speed').textContent = val(s.speedValid, s.speed, ' km/h');
  document.getElementById('coolant').textContent = val(s.coolantValid, s.coolant, ' °C');
  document.getElementById('throttle').textContent = val(s.throttleValid, s.throttle.toFixed(1), ' %');
  document.getElementById('fuel').textContent = val(s.fuelValid, s.fuel.toFixed(1), ' %');
  document.getElementById('wifiInfo').textContent = s.wifi;
  document.getElementById('statusLine').innerHTML = `RAW: <b>${s.rawEnabled ? 'on' : 'off'}</b> | Mudanças: <b>${s.onlyChanges ? 'on' : 'off'}</b> | OBD: <b>${s.obdEnabled ? 'on' : 'off'}</b> | CAN: <b>${s.canProfile}</b>`;
}

async function refreshFrames() {
  if (paused) return;
  const resp = await fetch(`/api/frames?since=${lastSeq}`);
  const payload = await resp.json();
  (payload.frames || []).forEach(addFrameRow);
  lastSeq = payload.lastSeq || lastSeq;
  applyFilter();
}

async function toggleFlag(name) {
  await fetch(`/api/toggle?name=${name}`);
  refreshStatus();
}

async function clearFrames() {
  await fetch('/api/clear');
  document.getElementById('frames').innerHTML = '';
  lastSeq = 0;
}

function togglePause() {
  paused = !paused;
  document.getElementById('pauseBtn').textContent = paused ? 'Retomar captura' : 'Pausar captura';
}

async function sendFrame(event) {
  event.preventDefault();
  const id = document.getElementById('txId').value.trim();
  const data = document.getElementById('txData').value.trim();
  const ext = document.getElementById('txType').value.trim() === '1' ? '1' : '0';
  const resp = await fetch(`/api/send?id=${encodeURIComponent(id)}&data=${encodeURIComponent(data)}&ext=${ext}`);
  const result = await resp.json();
  alert(result.message || (result.ok ? 'Frame enviado.' : 'Falha ao enviar.'));
}

document.getElementById('idFilter').addEventListener('input', applyFilter);
setInterval(refreshStatus, 1000);
setInterval(refreshFrames, 250);
refreshStatus();
refreshFrames();
</script>
</body>
</html>
)HTML";

void printHexByte(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

String formatCanId(uint32_t id, bool extended) {
  char buffer[12] = {0};
  snprintf(buffer, sizeof(buffer), extended ? "0x%08lX" : "0x%03lX", static_cast<unsigned long>(id));
  return String(buffer);
}

String formatDataBytes(const uint8_t *data, uint8_t len) {
  String text;
  text.reserve(24);

  for (uint8_t i = 0; i < len; i++) {
    if (data[i] < 0x10) {
      text += '0';
    }
    text += String(data[i], HEX);
    text.toUpperCase();
    if (i + 1 < len) {
      text += ' ';
    }
  }

  return text;
}

String jsonBool(bool value) {
  return value ? "true" : "false";
}

String wifiSummary() {
  String text = "AP: ";
  text += WIFI_AP_SSID;
  text += " -> http://";
  text += WiFi.softAPIP().toString();

  if (WiFi.status() == WL_CONNECTED) {
    text += " | STA: ";
    text += WiFi.localIP().toString();
  }

  return text;
}

void logFrame(uint32_t id, bool extended, bool rtr, uint8_t len, const uint8_t *data) {
  size_t index = 0;

  if (frameLogCount < MAX_LOGGED_FRAMES) {
    index = (frameLogHead + frameLogCount) % MAX_LOGGED_FRAMES;
    frameLogCount++;
  } else {
    index = frameLogHead;
    frameLogHead = (frameLogHead + 1) % MAX_LOGGED_FRAMES;
  }

  FrameLogEntry &entry = frameLog[index];
  entry.seq = ++frameSequence;
  entry.timestampMs = millis();
  entry.id = id;
  entry.extended = extended;
  entry.rtr = rtr;
  entry.len = len;
  memset(entry.data, 0, sizeof(entry.data));
  memcpy(entry.data, data, len);
}

void clearFrameLog() {
  frameLogHead = 0;
  frameLogCount = 0;
}

void printFrame(uint32_t id, bool extended, uint8_t len, const uint8_t *data) {
  Serial.printf("[CAN] %s DLC=%u DATA=%s\r\n",
                formatCanId(id, extended).c_str(),
                len,
                formatDataBytes(data, len).c_str());
}

bool shouldPrintFrame(uint32_t id, uint8_t len, const uint8_t *data) {
  const uint32_t now = millis();

  for (FrameTracker &tracker : trackedFrames) {
    if (tracker.used && tracker.id == id) {
      const bool changed = (tracker.len != len) || (memcmp(tracker.data, data, len) != 0);
      if (changed || (now - tracker.lastPrintMs >= RAW_REPRINT_MS)) {
        tracker.len = len;
        memcpy(tracker.data, data, len);
        tracker.lastPrintMs = now;
        return true;
      }
      return false;
    }
  }

  for (FrameTracker &tracker : trackedFrames) {
    if (!tracker.used) {
      tracker.used = true;
      tracker.id = id;
      tracker.len = len;
      memcpy(tracker.data, data, len);
      tracker.lastPrintMs = now;
      return true;
    }
  }

  return true;
}

void sendObdRequest(uint8_t pid) {
  uint8_t request[8] = {0x02, 0x01, pid, 0x00, 0x00, 0x00, 0x00, 0x00};
  const byte status = CAN.sendMsgBuf(0x7DF, 0, 8, request);

  if (status != CAN_OK) {
    const uint32_t now = millis();
    if (now - lastObdErrorMs >= 3000) {
      lastObdErrorMs = now;
      Serial.printf("[OBD] Sem TX/ACK na rede CAN (erro=%u). Verifique 500 kbps, CANH/CANL e cristal 8/16 MHz.\r\n", status);
    }
  }
}

void decodeObdFrame(uint32_t id, uint8_t len, const uint8_t *data) {
  if (id < 0x7E8 || id > 0x7EF || len < 4) {
    return;
  }

  if (data[1] != 0x41) {
    return;
  }

  const uint8_t pid = data[2];
  liveData.lastResponseMs = millis();

  switch (pid) {
    case 0x0C:
      if (len >= 5) {
        liveData.rpm = ((data[3] * 256.0f) + data[4]) / 4.0f;
        liveData.rpmValid = true;
      }
      break;

    case 0x0D:
      liveData.speed = data[3];
      liveData.speedValid = true;
      break;

    case 0x05:
      liveData.coolant = static_cast<int>(data[3]) - 40;
      liveData.coolantValid = true;
      break;

    case 0x11:
      liveData.throttle = (data[3] * 100.0f) / 255.0f;
      liveData.throttleValid = true;
      break;

    case 0x04:
      liveData.engineLoad = (data[3] * 100.0f) / 255.0f;
      liveData.loadValid = true;
      break;

    case 0x2F:
      liveData.fuelLevel = (data[3] * 100.0f) / 255.0f;
      liveData.fuelValid = true;
      break;

    case 0x0F:
      liveData.intakeTemp = static_cast<int>(data[3]) - 40;
      liveData.intakeValid = true;
      break;

    default:
      break;
  }
}

void decodeSniffedVehicleFrame(uint32_t id, uint8_t len, const uint8_t *data) {
  switch (id) {
    case 0x201:
      if (len >= 2) {
        const uint16_t rawRpm = (static_cast<uint16_t>(data[0]) << 8) | data[1];
        const float rpm = rawRpm / 4.0f;
        if (rpm >= 300.0f && rpm <= 8000.0f) {
          liveData.rpm = rpm;
          liveData.rpmValid = true;
          liveData.lastResponseMs = millis();
        }
      }

      if (len >= 7) {
        liveData.speed = data[6];
        liveData.speedValid = true;
        liveData.lastResponseMs = millis();
      }
      break;

    case 0x420:
      if (len >= 1) {
        const int coolant = static_cast<int>(data[0]) - 40;
        if (coolant >= -40 && coolant <= 150) {
          liveData.coolant = coolant;
          liveData.coolantValid = true;
          liveData.lastResponseMs = millis();
        }
      }
      break;

    default:
      break;
  }
}

bool parseHexValue(String input, uint32_t &value) {
  input.trim();
  input.replace("0x", "");
  input.replace("0X", "");

  if (input.isEmpty()) {
    return false;
  }

  char *endPtr = nullptr;
  value = strtoul(input.c_str(), &endPtr, 16);
  return endPtr != nullptr && *endPtr == '\0';
}

bool parseDataBytes(const String &input, uint8_t *data, uint8_t &len) {
  String compact;
  compact.reserve(input.length());

  for (size_t i = 0; i < input.length(); i++) {
    const char ch = input[i];
    if (isxdigit(static_cast<unsigned char>(ch))) {
      compact += ch;
    }
  }

  if (compact.isEmpty()) {
    len = 0;
    return true;
  }

  if ((compact.length() % 2) != 0 || compact.length() > 16) {
    return false;
  }

  len = compact.length() / 2;
  for (uint8_t i = 0; i < len; i++) {
    const String part = compact.substring(i * 2, i * 2 + 2);
    char *endPtr = nullptr;
    const unsigned long parsed = strtoul(part.c_str(), &endPtr, 16);
    if (endPtr == nullptr || *endPtr != '\0') {
      return false;
    }
    data[i] = static_cast<uint8_t>(parsed & 0xFF);
  }

  return true;
}

String buildStatusJson() {
  String json;
  json.reserve(420);

  json += "{";
  json += "\"rxCount\":" + String(rxFrameCount);
  json += ",\"lastSeq\":" + String(frameSequence);
  json += ",\"rawEnabled\":" + jsonBool(rawOutputEnabled);
  json += ",\"onlyChanges\":" + jsonBool(printOnlyChangedFrames);
  json += ",\"obdEnabled\":" + jsonBool(obdPollingEnabled);
  json += ",\"rpm\":" + String(liveData.rpm, 1);
  json += ",\"rpmValid\":" + jsonBool(liveData.rpmValid);
  json += ",\"speed\":" + String(liveData.speed);
  json += ",\"speedValid\":" + jsonBool(liveData.speedValid);
  json += ",\"coolant\":" + String(liveData.coolant);
  json += ",\"coolantValid\":" + jsonBool(liveData.coolantValid);
  json += ",\"throttle\":" + String(liveData.throttle, 1);
  json += ",\"throttleValid\":" + jsonBool(liveData.throttleValid);
  json += ",\"fuel\":" + String(liveData.fuelLevel, 1);
  json += ",\"fuelValid\":" + jsonBool(liveData.fuelValid);
  json += ",\"wifi\":\"" + wifiSummary() + "\"";
  json += ",\"canProfile\":\"500 kbps / 8 MHz | OBD + sniff 0x201/0x420\"";
  json += "}";

  return json;
}

String buildFramesJson(uint32_t sinceSeq) {
  String json;
  json.reserve(12000);
  json += "{\"lastSeq\":" + String(frameSequence) + ",\"frames\":[";

  bool first = true;
  for (size_t i = 0; i < frameLogCount; i++) {
    const FrameLogEntry &entry = frameLog[(frameLogHead + i) % MAX_LOGGED_FRAMES];
    if (entry.seq <= sinceSeq) {
      continue;
    }

    if (!first) {
      json += ',';
    }
    first = false;

    json += "{";
    json += "\"seq\":" + String(entry.seq);
    json += ",\"ts\":" + String(entry.timestampMs);
    json += ",\"id\":\"" + formatCanId(entry.id, entry.extended) + "\"";
    json += ",\"type\":\"" + String(entry.rtr ? "RTR" : (entry.extended ? "EXT" : "STD")) + "\"";
    json += ",\"dlc\":" + String(entry.len);
    json += ",\"data\":\"" + formatDataBytes(entry.data, entry.len) + "\"";
    json += "}";
  }

  json += "]}";
  return json;
}

void handleCanReceive() {
  while (CAN.checkReceive() == CAN_MSGAVAIL) {
    unsigned long canId = 0;
    uint8_t len = 0;
    uint8_t data[8] = {0};

    if (CAN.readMsgBuf(&canId, &len, data) != CAN_OK) {
      break;
    }

    const bool extended = canId > 0x7FF;
    rxFrameCount++;
    logFrame(canId, extended, false, len, data);
    decodeObdFrame(canId, len, data);
    decodeSniffedVehicleFrame(canId, len, data);

    if (rawOutputEnabled && (!printOnlyChangedFrames || shouldPrintFrame(canId, len, data))) {
      printFrame(canId, extended, len, data);
    }
  }
}

void printDashboard() {
  const uint32_t now = millis();
  if (now - lastDashboardMs < DASHBOARD_INTERVAL_MS) {
    return;
  }

  lastDashboardMs = now;
  Serial.print("[DASH] ");

  if (liveData.rpmValid) {
    Serial.printf("RPM=%.0f | ", liveData.rpm);
  } else {
    Serial.print("RPM=-- | ");
  }

  if (liveData.speedValid) {
    Serial.printf("VEL=%u km/h | ", liveData.speed);
  } else {
    Serial.print("VEL=-- | ");
  }

  if (liveData.coolantValid) {
    Serial.printf("TEMP=%d C | ", liveData.coolant);
  } else {
    Serial.print("TEMP=-- | ");
  }

  if (liveData.throttleValid) {
    Serial.printf("TPS=%.1f%% | ", liveData.throttle);
  } else {
    Serial.print("TPS=-- | ");
  }

  if (liveData.fuelValid) {
    Serial.printf("FUEL=%.1f%% | ", liveData.fuelLevel);
  } else {
    Serial.print("FUEL=-- | ");
  }

  Serial.printf("RX=%lu", static_cast<unsigned long>(rxFrameCount));

  if (liveData.lastResponseMs == 0 || (now - liveData.lastResponseMs > 4000)) {
    Serial.print(" | OBD sem resposta");
  }

  Serial.println();
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const char cmd = static_cast<char>(tolower(Serial.read()));

    switch (cmd) {
      case 'h':
        Serial.println("\r\nComandos: [r]=raw on/off | [c]=somente mudancas | [o]=OBD polling | [w]=wifi/url | [s]=status | [h]=ajuda");
        break;

      case 'r':
        rawOutputEnabled = !rawOutputEnabled;
        Serial.printf("[CFG] Raw CAN %s\r\n", rawOutputEnabled ? "ATIVADO" : "DESATIVADO");
        break;

      case 'c':
        printOnlyChangedFrames = !printOnlyChangedFrames;
        Serial.printf("[CFG] Filtro de mudancas %s\r\n", printOnlyChangedFrames ? "ATIVADO" : "DESATIVADO");
        break;

      case 'o':
        obdPollingEnabled = !obdPollingEnabled;
        Serial.printf("[CFG] OBD polling %s\r\n", obdPollingEnabled ? "ATIVADO" : "DESATIVADO");
        break;

      case 'w':
        Serial.printf("[WIFI] %s\r\n", wifiSummary().c_str());
        break;

      case 's':
        Serial.printf("[STATUS] RX=%lu | raw=%s | mudancas=%s | obd=%s | %s\r\n",
                      static_cast<unsigned long>(rxFrameCount),
                      rawOutputEnabled ? "on" : "off",
                      printOnlyChangedFrames ? "on" : "off",
                      obdPollingEnabled ? "on" : "off",
                      wifiSummary().c_str());
        break;

      default:
        break;
    }
  }
}

void requestNextPidIfNeeded() {
  if (!obdPollingEnabled) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastObdRequestMs < OBD_REQUEST_INTERVAL_MS) {
    return;
  }

  lastObdRequestMs = now;
  sendObdRequest(obdPidList[nextPidIndex]);
  nextPidIndex = (nextPidIndex + 1) % (sizeof(obdPidList) / sizeof(obdPidList[0]));
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatusApi() {
  server.send(200, "application/json", buildStatusJson());
}

void handleFramesApi() {
  uint32_t since = 0;
  if (server.hasArg("since")) {
    since = static_cast<uint32_t>(server.arg("since").toInt());
  }
  server.send(200, "application/json", buildFramesJson(since));
}

void handleToggleApi() {
  const String name = server.arg("name");

  if (name == "raw") {
    rawOutputEnabled = !rawOutputEnabled;
  } else if (name == "changes") {
    printOnlyChangedFrames = !printOnlyChangedFrames;
  } else if (name == "obd") {
    obdPollingEnabled = !obdPollingEnabled;
  }

  handleStatusApi();
}

void handleClearApi() {
  clearFrameLog();
  server.send(200, "application/json", "{\"ok\":true,\"message\":\"Buffer limpo.\"}");
}

void handleSendApi() {
  if (!server.hasArg("id")) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"Parametro id ausente.\"}");
    return;
  }

  uint32_t frameId = 0;
  if (!parseHexValue(server.arg("id"), frameId)) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"ID invalido.\"}");
    return;
  }

  uint8_t data[8] = {0};
  uint8_t len = 0;
  if (!parseDataBytes(server.arg("data"), data, len)) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"Dados invalidos. Use hex com até 8 bytes.\"}");
    return;
  }

  const byte ext = (server.arg("ext") == "1") ? 1 : 0;
  const byte status = CAN.sendMsgBuf(frameId, ext, len, data);

  if (status == CAN_OK) {
    server.send(200, "application/json", "{\"ok\":true,\"message\":\"Frame enviado com sucesso.\"}");
  } else {
    server.send(500, "application/json", "{\"ok\":false,\"message\":\"Falha no MCP2515 ao enviar frame.\"}");
  }
}

void setupWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  Serial.printf("[WIFI] AP ativo: %s | senha: %s | URL: http://%s\r\n",
                WIFI_AP_SSID,
                WIFI_AP_PASS,
                WiFi.softAPIP().toString().c_str());

  if (strlen(WIFI_STA_SSID) > 0) {
    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WIFI] Conectado na rede local: http://%s\r\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[WIFI] Nao conectou ao Wi-Fi local; usando somente o Access Point do ESP32.");
    }
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatusApi);
  server.on("/api/frames", HTTP_GET, handleFramesApi);
  server.on("/api/toggle", HTTP_GET, handleToggleApi);
  server.on("/api/clear", HTTP_GET, handleClearApi);
  server.on("/api/send", HTTP_GET, handleSendApi);
  server.onNotFound([]() {
    server.send(404, "text/plain", "404 - rota nao encontrada");
  });
  server.begin();
  Serial.println("[WEB] Interface pronta.");
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1500);

  pinMode(CAN_INT_PIN, INPUT_PULLUP);
  SPI.begin(CAN_SCK_PIN, CAN_MISO_PIN, CAN_MOSI_PIN, CAN_CS_PIN);

  Serial.println();
  Serial.println("====================================================");
  Serial.println(" ESP32 + MCP2515 | CAN Hacker Web Sniffer");
  Serial.println("====================================================");
  Serial.println("Pinos ESP32 -> MCP2515: SCK=18 MISO=19 MOSI=23 CS=5 INT=4");
  Serial.println("Padrao atual: CAN 500 kbps / cristal 8 MHz");
  Serial.println("Se nao houver trafego, teste 250 kbps ou ajuste para 16 MHz.");

  while (CAN.begin(MCP_ANY, CAN_BUS_SPEED, MCP2515_CLOCK) != CAN_OK) {
    Serial.println("[ERRO] MCP2515 nao inicializou. Verifique SPI, alimentacao e pino CS.");
    delay(1000);
  }

  CAN.setMode(MCP_NORMAL);
  setupWiFi();
  setupWebServer();

  Serial.println("[OK] MCP2515 iniciado em modo normal.");
  Serial.println("[INFO] Abra o navegador no IP mostrado acima para usar o sniffer web.");
  Serial.println("[INFO] Digite 'h' no monitor serial para ver os comandos.\r\n");
}

void loop() {
  handleSerialCommands();
  handleCanReceive();
  requestNextPidIfNeeded();
  printDashboard();
  server.handleClient();
}