#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>

// I2C pins (change if needed)
#define I2C_SDA 21
#define I2C_SCL 22

enum { CMD_READ = 1, CMD_WRITE = 2, CMD_RESP = 3 };

struct __attribute__((packed)) I2C_Packet {
  uint8_t cmd;
  uint8_t i2c_addr;
  uint8_t reg;
  uint8_t len;
  uint8_t data[200];
};

struct __attribute__((packed)) I2C_Response {
  uint8_t cmd;
  uint8_t status;
  uint8_t len;
  uint8_t data[200];
};

void ensurePeer(const uint8_t *mac) {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_err_t res = esp_now_add_peer(&peerInfo);
  // it's okay if peer already exists
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // optional: log
}

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len < 1) return;
  const I2C_Packet *pkt = (const I2C_Packet *)incomingData;
  I2C_Response resp;
  memset(&resp, 0, sizeof(resp));
  resp.cmd = CMD_RESP;

  if (pkt->cmd == CMD_READ) {
    uint8_t addr = pkt->i2c_addr;
    uint8_t reg = pkt->reg;
    uint8_t rlen = pkt->len;
    // write register pointer
    Wire.beginTransmission(addr);
    Wire.write(reg);
    uint8_t err = Wire.endTransmission(false);
    if (err != 0) {
      resp.status = 1;
      resp.len = 0;
    } else {
      uint8_t got = Wire.requestFrom((int)addr, (int)rlen);
      resp.status = 0;
      resp.len = got;
      for (int i=0;i<got && i<200;i++) resp.data[i] = Wire.read();
    }
  } else if (pkt->cmd == CMD_WRITE) {
    uint8_t addr = pkt->i2c_addr;
    uint8_t reg = pkt->reg;
    Wire.beginTransmission(addr);
    Wire.write(reg);
    for (int i=0;i<pkt->len;i++) Wire.write(pkt->data[i]);
    uint8_t err = Wire.endTransmission();
    resp.status = (err == 0) ? 0 : 1;
    resp.len = 0;
  } else {
    resp.status = 1;
    resp.len = 0;
  }

  // ensure peer exists and send back
  ensurePeer(mac);
  esp_err_t res = esp_now_send(mac, (uint8_t *)&resp, sizeof(I2C_Response));
  (void)res;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("ESP32 I2C wireless bridge - SLAVE");

  Wire.begin(); // default pins

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  String mac = WiFi.macAddress();
  Serial.print("This SLAVE MAC: "); Serial.println(mac);
  Serial.println("Keep this MAC and set it as peer on MASTER.\n");
}

void loop() {
  // nothing to do; all handled in callback
  delay(1000);
}
