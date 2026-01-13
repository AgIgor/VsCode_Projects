#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>

// I2C pins (change if needed)
#define I2C_SDA 21
#define I2C_SCL 22

// Simple packet definitions
enum { CMD_READ = 1, CMD_WRITE = 2, CMD_RESP = 3 };

struct __attribute__((packed)) I2C_Packet {
  uint8_t cmd;
  uint8_t i2c_addr;
  uint8_t reg;
  uint8_t len; // for write: length of data; for read: requested length
  uint8_t data[200];
};

struct __attribute__((packed)) I2C_Response {
  uint8_t cmd;
  uint8_t status; // 0 ok, 1 error
  uint8_t len;
  uint8_t data[200];
};

uint8_t peerMac[6] = {0,0,0,0,0,0}; // set after reading slave's MAC via serial
bool peerSet = false;

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  // handle response from slave
  if (len < 3) return;
  const I2C_Response *resp = (const I2C_Response *)incomingData;
  if (resp->cmd != CMD_RESP) return;
  Serial.print("Response status="); Serial.print(resp->status);
  Serial.print(" len="); Serial.println(resp->len);
  if (resp->len > 0) {
    Serial.print("Data:");
    for (int i=0;i<resp->len;i++) {
      Serial.print(" ");
      Serial.print(resp->data[i], HEX);
    }
    Serial.println();
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last packet send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

bool parseMAC(const String &s, uint8_t *out) {
  // expect AA:BB:CC:DD:EE:FF or AABBCCDDEEFF
  String t = s;
  t.replace(":", "");
  t.replace("-", "");
  if (t.length() != 12) return false;
  for (int i=0;i<6;i++) {
    String byteStr = t.substring(i*2, i*2+2);
    out[i] = (uint8_t) strtoul(byteStr.c_str(), NULL, 16);
  }
  return true;
}

void sendRead(uint8_t i2c_addr, uint8_t reg, uint8_t len) {
  if (!peerSet) {
    Serial.println("Peer MAC not set. Provide slave MAC with: peer AA:BB:CC:DD:EE:FF");
    return;
  }
  I2C_Packet pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.cmd = CMD_READ;
  pkt.i2c_addr = i2c_addr;
  pkt.reg = reg;
  pkt.len = len;
  esp_err_t res = esp_now_send(peerMac, (uint8_t *)&pkt, sizeof(I2C_Packet));
  if (res != ESP_OK) {
    Serial.printf("esp_now_send failed: %d\n", res);
  }
}

void sendWrite(uint8_t i2c_addr, uint8_t reg, const uint8_t *data, uint8_t len) {
  if (!peerSet) {
    Serial.println("Peer MAC not set. Provide slave MAC with: peer AA:BB:CC:DD:EE:FF");
    return;
  }
  I2C_Packet pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.cmd = CMD_WRITE;
  pkt.i2c_addr = i2c_addr;
  pkt.reg = reg;
  pkt.len = len;
  if (len) memcpy(pkt.data, data, len);
  esp_err_t res = esp_now_send(peerMac, (uint8_t *)&pkt, sizeof(I2C_Packet));
  if (res != ESP_OK) {
    Serial.printf("esp_now_send failed: %d\n", res);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 I2C wireless bridge - MASTER");

  Wire.begin(); // uses default SDA/SCL pins (21/22) on many boards, or change with Wire.begin(SDA,SCL)

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // print MAC to help pairing from slave
  String mac = WiFi.macAddress();
  Serial.print("This MASTER MAC: "); Serial.println(mac);
  Serial.println("On the SLAVE run it and copy its MAC. Then in this serial use: peer AA:BB:...\n");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;
    if (line.startsWith("peer ")) {
      String macstr = line.substring(5);
      if (parseMAC(macstr, peerMac)) {
        peerSet = true;
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, peerMac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
        Serial.print("Peer set: "); Serial.println(macstr);
      } else {
        Serial.println("Invalid MAC format");
      }
    } else if (line.startsWith("r ")) {
      // r addr reg len  (hex or decimal)
      uint32_t a, r, l;
      if (sscanf(line.c_str()+2, "%i %i %i", &a, &r, &l) == 3) {
        sendRead((uint8_t)a, (uint8_t)r, (uint8_t)l);
      } else {
        Serial.println("Usage: r addr reg len\nExample: r 0x76 0xD0 1");
      }
    } else if (line.startsWith("w ")) {
      // w addr reg b1 b2 ...
      // naive parser
      const char *s = line.c_str()+2;
      int parsed = 0;
      unsigned int vals[64];
      char *p = NULL;
      char buf[256];
      strncpy(buf, s, sizeof(buf)-1); buf[sizeof(buf)-1]=0;
      char *tok = strtok(buf, " \t");
      while (tok && parsed < 64) {
        vals[parsed++] = (unsigned int)strtoul(tok, &p, 0);
        tok = strtok(NULL, " \t");
      }
      if (parsed >= 2) {
        uint8_t addr = (uint8_t)vals[0];
        uint8_t reg = (uint8_t)vals[1];
        uint8_t data[200];
        int dlen = parsed - 2;
        for (int i=0;i<dlen;i++) data[i] = (uint8_t)vals[2+i];
        sendWrite(addr, reg, data, dlen);
      } else {
        Serial.println("Usage: w addr reg b1 b2 ...\nExample: w 0x40 0x00 0x12 0x34");
      }
    } else {
      Serial.println("Commands:\n peer AA:BB:CC:DD:EE:FF\n r addr reg len\n w addr reg b1 b2 ...");
    }
  }
  delay(10);
}
