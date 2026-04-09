#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>
#include <cstring>
#include <cctype>

// ===============================
// Ajuste os pinos conforme sua montagem
// ESP32 DevKit V1 + MCP2515 (SPI)
// SCK=18 | MISO=19 | MOSI=23 | CS=5 | INT=4
// ===============================
constexpr uint8_t CAN_CS_PIN = 5;
constexpr uint8_t CAN_INT_PIN = 4;
constexpr uint8_t CAN_SCK_PIN = 18;
constexpr uint8_t CAN_MISO_PIN = 19;
constexpr uint8_t CAN_MOSI_PIN = 23;

constexpr uint32_t SERIAL_BAUD = 230400;
constexpr uint8_t MCP2515_CLOCK = MCP_8MHZ;   // troque para MCP_16MHZ se o módulo usar cristal de 16 MHz
constexpr uint8_t CAN_BUS_SPEED = CAN_500KBPS; // Fiesta/OBD CAN costuma usar 500 kbps

constexpr uint32_t OBD_REQUEST_INTERVAL_MS = 350;
constexpr uint32_t DASHBOARD_INTERVAL_MS = 1000;
constexpr uint32_t RAW_REPRINT_MS = 500;
constexpr size_t MAX_TRACKED_IDS = 32;

MCP_CAN CAN(CAN_CS_PIN);

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

FrameTracker trackedFrames[MAX_TRACKED_IDS];

bool rawOutputEnabled = true;
bool printOnlyChangedFrames = true;
bool obdPollingEnabled = true;
uint32_t rxFrameCount = 0;
uint32_t lastObdRequestMs = 0;
uint32_t lastObdErrorMs = 0;
uint32_t lastDashboardMs = 0;
size_t nextPidIndex = 0;

const uint8_t obdPidList[] = {
  0x0C, // RPM
  0x0D, // velocidade
  0x05, // temperatura do motor
  0x11, // throttle
  0x04, // engine load
  0x2F, // nível de combustível
  0x0F  // temperatura do ar de admissão
};

void printHexByte(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

void printFrame(uint32_t id, uint8_t len, const uint8_t *data) {
  Serial.printf("[CAN] ID=0x%03lX DLC=%u DATA=", static_cast<unsigned long>(id), len);
  for (uint8_t i = 0; i < len; i++) {
    printHexByte(data[i]);
    if (i + 1 < len) {
      Serial.print(' ');
    }
  }
  Serial.println();
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

void handleCanReceive() {
  while (CAN.checkReceive() == CAN_MSGAVAIL) {
    unsigned long canId = 0;
    uint8_t len = 0;
    uint8_t data[8] = {0};

    if (CAN.readMsgBuf(&canId, &len, data) != CAN_OK) {
      break;
    }

    rxFrameCount++;
    decodeObdFrame(canId, len, data);

    if (rawOutputEnabled && (!printOnlyChangedFrames || shouldPrintFrame(canId, len, data))) {
      printFrame(canId, len, data);
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

  if (liveData.loadValid) {
    Serial.printf("LOAD=%.1f%% | ", liveData.engineLoad);
  } else {
    Serial.print("LOAD=-- | ");
  }

  if (liveData.fuelValid) {
    Serial.printf("FUEL=%.1f%% | ", liveData.fuelLevel);
  } else {
    Serial.print("FUEL=-- | ");
  }

  if (liveData.intakeValid) {
    Serial.printf("IAT=%d C | ", liveData.intakeTemp);
  } else {
    Serial.print("IAT=-- | ");
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
        Serial.println("\r\nComandos: [r]=raw on/off | [c]=somente mudancas | [o]=OBD polling | [s]=status | [h]=ajuda");
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

      case 's':
        Serial.printf("[STATUS] RX=%lu | raw=%s | mudancas=%s | obd=%s\r\n",
                      static_cast<unsigned long>(rxFrameCount),
                      rawOutputEnabled ? "on" : "off",
                      printOnlyChangedFrames ? "on" : "off",
                      obdPollingEnabled ? "on" : "off");
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

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1500);

  pinMode(CAN_INT_PIN, INPUT_PULLUP);
  SPI.begin(CAN_SCK_PIN, CAN_MISO_PIN, CAN_MOSI_PIN, CAN_CS_PIN);

  Serial.println();
  Serial.println("====================================================");
  Serial.println(" ESP32 + MCP2515 | Monitor CAN Ford Fiesta 2005");
  Serial.println("====================================================");
  Serial.println("Pinos ESP32 -> MCP2515: SCK=18 MISO=19 MOSI=23 CS=5 INT=4");
  Serial.println("Padrao atual: CAN 500 kbps / cristal 8 MHz");
  Serial.println("Se nao houver trafego, teste 250 kbps ou ajuste para 16 MHz.");

  while (CAN.begin(MCP_ANY, CAN_BUS_SPEED, MCP2515_CLOCK) != CAN_OK) {
    Serial.println("[ERRO] MCP2515 nao inicializou. Verifique SPI, 5V/3V3 e o pino CS.");
    delay(1000);
  }

  CAN.setMode(MCP_NORMAL);

  Serial.println("[OK] MCP2515 iniciado em modo normal.");
  Serial.println("[INFO] O sistema vai mostrar quadros CAN e tentar ler PIDs OBD-II padrao.");
  Serial.println("[INFO] Digite 'h' no monitor serial para ver os comandos.\r\n");
}

void loop() {
  handleSerialCommands();
  handleCanReceive();
  requestNextPidIfNeeded();
  printDashboard();
}