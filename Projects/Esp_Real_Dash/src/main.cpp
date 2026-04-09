#include <Arduino.h>
#include <BluetoothSerial.h>

BluetoothSerial ELM327;

struct TelemetryData {
  float rpm = 900.0f;
  float speedKmh = 0.0f;
  float coolantC = 80.0f;
  float intakeC = 28.0f;
  float engineLoadPct = 12.0f;
  float throttlePct = 0.0f;
  float mapKpa = 35.0f;
  float mafGps = 3.5f;
  float timingAdvanceDeg = 8.0f;
  float oilTempC = 85.0f;
  float batteryV = 13.8f;
  float fuelPct = 75.0f;
  uint16_t runTimeSec = 120;
  bool leftSignal = false;
  bool rightSignal = false;
  bool headlights = false;
  bool checkEngine = false;
  bool doorOpen = false;
  bool brake = false;
};

TelemetryData telemetry;

String serialLine;
String btLine;

unsigned long lastStatusPrint = 0;

static inline uint8_t clampToByte(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return static_cast<uint8_t>(value);
}

String byteToHex(uint8_t value) {
  String out = String(value, HEX);
  out.toUpperCase();
  if (out.length() < 2) {
    out = "0" + out;
  }
  return out;
}

String maskToHexBytes(uint32_t mask) {
  uint8_t b1 = (mask >> 24) & 0xFF;
  uint8_t b2 = (mask >> 16) & 0xFF;
  uint8_t b3 = (mask >> 8) & 0xFF;
  uint8_t b4 = mask & 0xFF;
  return byteToHex(b1) + " " + byteToHex(b2) + " " + byteToHex(b3) + " " + byteToHex(b4);
}

uint32_t pidMask(uint8_t basePid, const uint8_t *pids, size_t count) {
  uint32_t mask = 0;
  for (size_t i = 0; i < count; i++) {
    uint8_t pid = pids[i];
    if (pid < basePid || pid > static_cast<uint8_t>(basePid + 31)) {
      continue;
    }
    uint8_t index = pid - basePid;
    uint8_t bitPos = 31 - index;
    mask |= (1UL << bitPos);
  }
  return mask;
}

void sendElmResponse(const String &line) {
  ELM327.print(line);
  ELM327.print("\r>");
}

void parseTelemetryLine(String line) {
  line.trim();
  if (line.isEmpty()) {
    return;
  }

  line.replace(';', ',');
  int start = 0;

  while (start < static_cast<int>(line.length())) {
    int commaIndex = line.indexOf(',', start);
    if (commaIndex == -1) {
      commaIndex = line.length();
    }

    String token = line.substring(start, commaIndex);
    token.trim();

    int eqIndex = token.indexOf('=');
    if (eqIndex > 0) {
      String key = token.substring(0, eqIndex);
      String valueStr = token.substring(eqIndex + 1);
      key.trim();
      valueStr.trim();
      key.toUpperCase();

      float value = valueStr.toFloat();

      if (key == "RPM") {
        telemetry.rpm = max(0.0f, value);
      } else if (key == "SPEED" || key == "SPD" || key == "VSS") {
        telemetry.speedKmh = constrain(value, 0.0f, 255.0f);
      } else if (key == "LOAD") {
        telemetry.engineLoadPct = constrain(value, 0.0f, 100.0f);
      } else if (key == "COOLANT" || key == "ECT") {
        telemetry.coolantC = constrain(value, -40.0f, 215.0f);
      } else if (key == "IAT") {
        telemetry.intakeC = constrain(value, -40.0f, 215.0f);
      } else if (key == "THROTTLE" || key == "TPS") {
        telemetry.throttlePct = constrain(value, 0.0f, 100.0f);
      } else if (key == "MAP") {
        telemetry.mapKpa = constrain(value, 0.0f, 255.0f);
      } else if (key == "MAF") {
        telemetry.mafGps = constrain(value, 0.0f, 655.35f);
      } else if (key == "ADV" || key == "SPARK") {
        telemetry.timingAdvanceDeg = constrain(value, -64.0f, 63.5f);
      } else if (key == "OILTEMP" || key == "EOT") {
        telemetry.oilTempC = constrain(value, -40.0f, 215.0f);
      } else if (key == "VOLT" || key == "VOLTAGE" || key == "VBAT") {
        telemetry.batteryV = constrain(value, 0.0f, 65.0f);
      } else if (key == "FUEL") {
        telemetry.fuelPct = constrain(value, 0.0f, 100.0f);
      } else if (key == "RUNTIME" || key == "RUN") {
        telemetry.runTimeSec = static_cast<uint16_t>(constrain(value, 0.0f, 65535.0f));
      } else if (key == "LEFT" || key == "LEFT_SIGNAL" || key == "SETAE" || key == "SETA_E") {
        telemetry.leftSignal = value >= 0.5f;
      } else if (key == "RIGHT" || key == "RIGHT_SIGNAL" || key == "SETAD" || key == "SETA_D") {
        telemetry.rightSignal = value >= 0.5f;
      } else if (key == "HEADLIGHT" || key == "LIGHT" || key == "HEADLIGHTS" || key == "FAROL" || key == "FAROIS") {
        telemetry.headlights = value >= 0.5f;
      } else if (key == "CEL" || key == "MIL" || key == "CHECKENGINE" || key == "INJECAO") {
        telemetry.checkEngine = value >= 0.5f;
      } else if (key == "DOOR" || key == "DOOROPEN" || key == "PORTA") {
        telemetry.doorOpen = value >= 0.5f;
      } else if (key == "BRAKE" || key == "FREIO") {
        telemetry.brake = value >= 0.5f;
      }
    }

    start = commaIndex + 1;
  }
}

String obdPidResponse(const String &cmd) {
  if (cmd == "0100") {
    const uint8_t supported[] = {0x01, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x1F, 0x20};
    return "41 00 " + maskToHexBytes(pidMask(0x01, supported, sizeof(supported)));
  }

  if (cmd == "0101") {
    uint8_t a = telemetry.checkEngine ? 0x81 : 0x00;
    return "41 01 " + byteToHex(a) + " 00 00 00";
  }

  if (cmd == "0120") {
    const uint8_t supported[] = {0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x40};
    return "41 20 " + maskToHexBytes(pidMask(0x21, supported, sizeof(supported)));
  }

  if (cmd == "0140") {
    const uint8_t supported[] = {0x42, 0x5C, 0x60};
    return "41 40 " + maskToHexBytes(pidMask(0x41, supported, sizeof(supported)));
  }

  if (cmd == "0104") {
    uint8_t a = clampToByte(static_cast<int>((telemetry.engineLoadPct / 100.0f) * 255.0f));
    return "41 04 " + byteToHex(a);
  }

  if (cmd == "010C") {
    int rpmRaw = static_cast<int>(telemetry.rpm * 4.0f);
    uint8_t a = (rpmRaw >> 8) & 0xFF;
    uint8_t b = rpmRaw & 0xFF;
    return "41 0C " + byteToHex(a) + " " + byteToHex(b);
  }

  if (cmd == "010D") {
    uint8_t a = clampToByte(static_cast<int>(telemetry.speedKmh));
    return "41 0D " + byteToHex(a);
  }

  if (cmd == "0105") {
    uint8_t a = clampToByte(static_cast<int>(telemetry.coolantC + 40.0f));
    return "41 05 " + byteToHex(a);
  }

  if (cmd == "010E") {
    uint8_t a = clampToByte(static_cast<int>((telemetry.timingAdvanceDeg + 64.0f) * 2.0f));
    return "41 0E " + byteToHex(a);
  }

  if (cmd == "010F") {
    uint8_t a = clampToByte(static_cast<int>(telemetry.intakeC + 40.0f));
    return "41 0F " + byteToHex(a);
  }

  if (cmd == "0110") {
    int mafRaw = static_cast<int>(telemetry.mafGps * 100.0f);
    uint8_t a = (mafRaw >> 8) & 0xFF;
    uint8_t b = mafRaw & 0xFF;
    return "41 10 " + byteToHex(a) + " " + byteToHex(b);
  }

  if (cmd == "0111") {
    uint8_t a = clampToByte(static_cast<int>((telemetry.throttlePct / 100.0f) * 255.0f));
    return "41 11 " + byteToHex(a);
  }

  if (cmd == "010B") {
    uint8_t a = clampToByte(static_cast<int>(telemetry.mapKpa));
    return "41 0B " + byteToHex(a);
  }

  if (cmd == "012F") {
    uint8_t a = clampToByte(static_cast<int>((telemetry.fuelPct / 100.0f) * 255.0f));
    return "41 2F " + byteToHex(a);
  }

  if (cmd == "0130") {
    return "41 30 " + byteToHex(telemetry.leftSignal ? 1 : 0);
  }

  if (cmd == "0131") {
    return "41 31 " + byteToHex(telemetry.rightSignal ? 1 : 0);
  }

  if (cmd == "0132") {
    return "41 32 " + byteToHex(telemetry.headlights ? 1 : 0);
  }

  if (cmd == "0133") {
    return "41 33 " + byteToHex(telemetry.doorOpen ? 1 : 0);
  }

  if (cmd == "0134") {
    return "41 34 " + byteToHex(telemetry.brake ? 1 : 0);
  }

  if (cmd == "0135") {
    return "41 35 " + byteToHex(telemetry.checkEngine ? 1 : 0);
  }

  if (cmd == "013A") {
    uint8_t flags = 0;
    if (telemetry.leftSignal) flags |= (1 << 0);
    if (telemetry.rightSignal) flags |= (1 << 1);
    if (telemetry.headlights) flags |= (1 << 2);
    if (telemetry.doorOpen) flags |= (1 << 3);
    if (telemetry.brake) flags |= (1 << 4);
    if (telemetry.checkEngine) flags |= (1 << 5);
    return "41 3A " + byteToHex(flags);
  }

  if (cmd == "011F") {
    uint8_t a = (telemetry.runTimeSec >> 8) & 0xFF;
    uint8_t b = telemetry.runTimeSec & 0xFF;
    return "41 1F " + byteToHex(a) + " " + byteToHex(b);
  }

  if (cmd == "0142") {
    int mv = static_cast<int>(telemetry.batteryV * 1000.0f);
    uint8_t a = (mv >> 8) & 0xFF;
    uint8_t b = mv & 0xFF;
    return "41 42 " + byteToHex(a) + " " + byteToHex(b);
  }

  if (cmd == "015C") {
    uint8_t a = clampToByte(static_cast<int>(telemetry.oilTempC + 40.0f));
    return "41 5C " + byteToHex(a);
  }

  if (cmd == "221001") {
    return "62 10 01 " + byteToHex(telemetry.leftSignal ? 1 : 0);
  }

  if (cmd == "221002") {
    return "62 10 02 " + byteToHex(telemetry.rightSignal ? 1 : 0);
  }

  if (cmd == "221003") {
    return "62 10 03 " + byteToHex(telemetry.headlights ? 1 : 0);
  }

  if (cmd == "221004") {
    return "62 10 04 " + byteToHex(telemetry.doorOpen ? 1 : 0);
  }

  if (cmd == "221005") {
    return "62 10 05 " + byteToHex(telemetry.brake ? 1 : 0);
  }

  if (cmd == "221006") {
    return "62 10 06 " + byteToHex(telemetry.checkEngine ? 1 : 0);
  }

  if (cmd == "22100A") {
    uint8_t flags = 0;
    if (telemetry.leftSignal) flags |= (1 << 0);
    if (telemetry.rightSignal) flags |= (1 << 1);
    if (telemetry.headlights) flags |= (1 << 2);
    if (telemetry.doorOpen) flags |= (1 << 3);
    if (telemetry.brake) flags |= (1 << 4);
    if (telemetry.checkEngine) flags |= (1 << 5);
    return "62 10 0A " + byteToHex(flags);
  }

  return "NO DATA";
}

void handleElmCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  cmd.replace(" ", "");

  if (cmd.isEmpty()) {
    sendElmResponse("");
    return;
  }

  Serial.print("BT CMD: ");
  Serial.println(cmd);

  if (cmd == "ATZ") {
    sendElmResponse("ELM327 v1.5");
    return;
  }

  if (cmd == "ATI") {
    sendElmResponse("ELM327 v1.5");
    return;
  }

  if (cmd == "ATRV") {
    sendElmResponse(String(telemetry.batteryV, 2) + "V");
    return;
  }

  if (cmd == "ATE0" || cmd == "ATL0" || cmd == "ATS0" || cmd == "ATH0" || cmd == "ATSP0" ||
      cmd == "ATD" || cmd == "ATAT1" || cmd == "ATSTFF") {
    sendElmResponse("OK");
    return;
  }

  if (cmd.startsWith("01")) {
    String response = obdPidResponse(cmd);
    Serial.print("BT RSP: ");
    Serial.println(response);
    sendElmResponse(response);
    return;
  }

  if (cmd.startsWith("22")) {
    String response = obdPidResponse(cmd);
    Serial.print("BT RSP: ");
    Serial.println(response);
    sendElmResponse(response);
    return;
  }

  sendElmResponse("OK");
}

void setup() {
  Serial.begin(115200);
  ELM327.begin("ESP32-ELM327");

  Serial.println("ESP32 RealDash ELM327 simulator");
  Serial.println("Envie na serial: RPM=1500,SPEED=40,LOAD=25,ECT=86,IAT=33,TPS=20,MAP=45,MAF=6.2,ADV=12,VBAT=13.8,FUEL=70,RUNTIME=321,OILTEMP=92,LEFT=1,RIGHT=0,HEADLIGHT=1,CEL=0,DOOR=0,BRAKE=0");
}

void loop() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (!serialLine.isEmpty()) {
        parseTelemetryLine(serialLine);
        serialLine = "";
      }
    } else {
      serialLine += c;
    }
  }

  while (ELM327.available()) {
    char c = static_cast<char>(ELM327.read());
    if (c == '\r' || c == '\n') {
      if (!btLine.isEmpty()) {
        handleElmCommand(btLine);
        btLine = "";
      }
    } else {
      btLine += c;
    }
  }

  if (millis() - lastStatusPrint > 1000) {
    lastStatusPrint = millis();
    Serial.print("TEL -> RPM:");
    Serial.print(telemetry.rpm, 0);
    Serial.print(" SPD:");
    Serial.print(telemetry.speedKmh, 0);
    Serial.print(" ECT:");
    Serial.print(telemetry.coolantC, 1);
    Serial.print(" IAT:");
    Serial.print(telemetry.intakeC, 1);
    Serial.print(" LOAD:");
    Serial.print(telemetry.engineLoadPct, 1);
    Serial.print(" TPS:");
    Serial.print(telemetry.throttlePct, 1);
    Serial.print(" MAP:");
    Serial.print(telemetry.mapKpa, 1);
    Serial.print(" MAF:");
    Serial.print(telemetry.mafGps, 2);
    Serial.print(" ADV:");
    Serial.print(telemetry.timingAdvanceDeg, 1);
    Serial.print(" OILT:");
    Serial.print(telemetry.oilTempC, 1);
    Serial.print(" VBAT:");
    Serial.print(telemetry.batteryV, 2);
    Serial.print(" FUEL:");
    Serial.print(telemetry.fuelPct, 1);
    Serial.print(" RUN:");
    Serial.print(telemetry.runTimeSec);
    Serial.print(" L:");
    Serial.print(telemetry.leftSignal ? 1 : 0);
    Serial.print(" R:");
    Serial.print(telemetry.rightSignal ? 1 : 0);
    Serial.print(" H:");
    Serial.print(telemetry.headlights ? 1 : 0);
    Serial.print(" CEL:");
    Serial.print(telemetry.checkEngine ? 1 : 0);
    Serial.print(" D:");
    Serial.print(telemetry.doorOpen ? 1 : 0);
    Serial.print(" B:");
    Serial.println(telemetry.brake ? 1 : 0);
  }
}