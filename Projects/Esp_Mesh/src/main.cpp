#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Primary Master Key (16 bytes) - change this to your secret (must be 16 chars)
const char *PMK = "0123456789ABCDEF"; // 16 chars

// Message types
enum MsgType : uint8_t { MSG_REG = 0, MSG_CMD = 1, MSG_ACK = 2, MSG_PAIR = 3, MSG_PAIR_ACK = 4 };
// Add a status telemetry message
enum { MSG_STATUS = 5 };

typedef struct __attribute__((packed)) {
  uint8_t type;      // MsgType
  uint8_t device_id; // target or source device id
  uint8_t seq;       // sequence number
  uint8_t outputs;   // 5 LSB bits used for outputs
  uint8_t inputs;    // used in ACK: 2 LSB bits
} espnow_message_t;

// Peer storage for master
struct Peer { uint8_t mac[6]; uint8_t device_id; bool used; };
Peer peers[10];

uint8_t seqCounter = 1;
// telemetry interval for slaves (ms)
const unsigned long TELEMETRY_INTERVAL_MS = 5000;
unsigned long lastTelemetryTs = 0;

// GPIO configuration (change to match your HW)
const uint8_t OUTPUT_PINS[5] = {2, 13, 14, 27, 26};
const uint8_t INPUT_PINS[2]  = {34, 35};
const uint8_t BOOT_PIN = 0; // DOIT devkit: GPIO0 (BOOT) pulled LOW when pressed

// Helpers
void printMac(const uint8_t *mac) {
  for (int i = 0; i < 6; i++) {
    if (i) Serial.print(":");
    Serial.printf("%02X", mac[i]);
  }
}

int findPeerIndexByDeviceId(uint8_t id) {
  for (int i = 0; i < 10; ++i) if (peers[i].used && peers[i].device_id == id) return i;
  return -1;
}

int findPeerIndexByMac(const uint8_t *mac) {
  for (int i = 0; i < 10; ++i) if (peers[i].used && memcmp(peers[i].mac, mac, 6) == 0) return i;
  return -1;
}

int addPeer(const uint8_t *mac, uint8_t device_id, bool encrypt = true) {
  int idx = findPeerIndexByMac(mac);
  if (idx >= 0) return idx;
  for (int i = 0; i < 10; ++i) {
    if (!peers[i].used) {
      memcpy(peers[i].mac, mac, 6);
      peers[i].device_id = device_id;
      peers[i].used = true;

      esp_now_peer_info_t peerInfo;
      memset(&peerInfo, 0, sizeof(peerInfo));
      memcpy(peerInfo.peer_addr, mac, 6);
      peerInfo.channel = 0;
      peerInfo.encrypt = encrypt ? 1 : 0; // use PMK when requested
      esp_err_t r = esp_now_add_peer(&peerInfo);
      Serial.printf("Added peer idx=%d device=%d ", i, device_id);
      printMac(mac);
      Serial.printf(" -> esp_now_add_peer=%d encrypt=%d\n", r, peerInfo.encrypt);
      return i;
    }
  }
  Serial.println("Peer table full");
  return -1;
}

// Broadcast MAC
uint8_t broadcastAddress[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// Pairing state (used on slaves when master initiates pairing)
bool pairingRequested = false;
uint8_t pairingRequesterMac[6];
uint8_t pairingRequesterDeviceId = 0;
unsigned long pairingRequestTs = 0;
const unsigned long PAIR_WINDOW_MS = 10000; // 10 seconds

// Called on master and slave when a packet is received
void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len < (int)sizeof(espnow_message_t)) return;
  espnow_message_t msg;
  memcpy(&msg, incomingData, sizeof(msg));

  if (msg.type == MSG_REG) {
    Serial.printf("[RECV] REG from "); printMac(mac); Serial.printf(" id=%d\n", msg.device_id);
    addPeer(mac, msg.device_id);
    return;
  }

    if (msg.type == MSG_PAIR) {
    Serial.printf("[RECV] PAIR from "); printMac(mac); Serial.printf(" id=%d\n", msg.device_id);
    // Master initiates: slave should request user confirmation (press BOOT)
  #ifdef ROLE_SLAVE
      // add master as temporary unencrypted peer so we can reply with PAIR_ACK
      addPeer(mac, msg.device_id, false);
      // store requester and open pairing window
      memcpy(pairingRequesterMac, mac, 6);
      pairingRequesterDeviceId = msg.device_id;
      pairingRequestTs = millis();
      pairingRequested = true;
      Serial.println("Pareamento solicitado: pressione BOOT neste dispositivo para confirmar (10s)");
  #else
    // Master does not auto-add peers on receiving its own PAIR
    Serial.println("Master received PAIR (ignored)");
  #endif
    return;
    }

  if (msg.type == MSG_PAIR_ACK) {
    Serial.printf("[RECV] PAIR_ACK from "); printMac(mac); Serial.printf(" id=%d\n", msg.device_id);
    addPeer(mac, msg.device_id);
    return;
  }

  if (msg.type == MSG_CMD) {
    Serial.printf("[RECV] CMD from "); printMac(mac); Serial.printf(" target=%d seq=%d outputs=0x%02X\n", msg.device_id, msg.seq, msg.outputs);
#ifdef ROLE_SLAVE
    // Only slaves act on CMD
    // If command targeted to this device (DEVICE_ID) or 0 for broadcast
#ifdef DEVICE_ID
    if (msg.device_id == DEVICE_ID || msg.device_id == 0) {
      // apply outputs
      for (int i = 0; i < 5; ++i) {
        bool level = (msg.outputs >> i) & 0x01;
        digitalWrite(OUTPUT_PINS[i], level ? HIGH : LOW);
      }
      // read inputs
      uint8_t inputs = 0;
      for (int i = 0; i < 2; ++i) {
        bool v = digitalRead(INPUT_PINS[i]);
        inputs |= (v ? 1 : 0) << i;
      }
      // send ACK back to sender
      espnow_message_t ack{};
      ack.type = MSG_ACK;
      ack.device_id = DEVICE_ID;
      ack.seq = msg.seq;
      ack.outputs = 0;
      ack.inputs = inputs;
      esp_err_t r = esp_now_send(mac, (uint8_t*)&ack, sizeof(ack));
      Serial.printf("Sent ACK to "); printMac(mac); Serial.printf(" result=%d inputs=0x%02X\n", r, inputs);
    }
#endif
#endif
    return;
  }

  if (msg.type == MSG_ACK) {
    Serial.printf("[RECV] ACK from "); printMac(mac); Serial.printf(" src=%d seq=%d inputs=0x%02X\n", msg.device_id, msg.seq, msg.inputs);
    // master can use this to confirm command execution
    return;
  }

  if (msg.type == MSG_STATUS) {
    Serial.printf("[RECV] STATUS from "); printMac(mac); Serial.printf(" id=%d seq=%d outputs=0x%02X inputs=0x%02X\n", msg.device_id, msg.seq, msg.outputs, msg.inputs);
    return;
  }
}

// send callback
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Send to "); printMac(mac_addr); Serial.printf(" status=%d\n", status);
}

// Ensure broadcast peer exists (required on some firmwares for broadcast to work)
void ensureBroadcastPeerAdded(bool encrypt) {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = encrypt ? 1 : 0;
  esp_err_t r = esp_now_add_peer(&peerInfo);
  if (r == ESP_OK) {
    Serial.println("Broadcast peer added");
  } else if (r == ESP_ERR_ESPNOW_EXIST) {
    Serial.println("Broadcast peer already exists");
  } else {
    Serial.printf("Failed adding broadcast peer: %d\n", r);
  }
}

#ifdef ROLE_MASTER
// Master-specific helpers
void sendCommandToDevice(uint8_t target_id, uint8_t outputs) {
  int idx = findPeerIndexByDeviceId(target_id);
  if (idx < 0) {
    Serial.printf("Unknown device %d\n", target_id);
    return;
  }
  espnow_message_t msg{};
  msg.type = MSG_CMD;
  msg.device_id = target_id;
  msg.seq = seqCounter++;
  msg.outputs = outputs & 0x1F;
  msg.inputs = 0;
  esp_err_t r = esp_now_send(peers[idx].mac, (uint8_t*)&msg, sizeof(msg));
  Serial.printf("CMD -> id=%d seq=%d outputs=0x%02X send_res=%d\n", target_id, msg.seq, msg.outputs, r);
}

void setupMaster() {
  Serial.println("ROLE: MASTER");
  // configure BOOT pin to detect pairing press
  pinMode(BOOT_PIN, INPUT_PULLUP);
  bool pairingMode = (digitalRead(BOOT_PIN) == LOW);

  // init WiFi + ESP-NOW
  WiFi.mode(WIFI_STA);
  esp_err_t r = esp_now_init();
  Serial.printf("esp_now_init=%d\n", r);
  esp_now_set_pmk((const uint8_t*)PMK);
  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  // Ensure broadcast peer exists (required on some firmwares)
  ensureBroadcastPeerAdded(false);

  // empty peer table; peers will be added upon registration
  for (int i = 0; i < 10; ++i) peers[i].used = false;

  if (pairingMode) {
    Serial.println("BOOT pressionado -> iniciando modo pareamento (MASTER) por 10s...");
    // send PAIR broadcasts for a window so slaves with BOOT pressed can pair
    espnow_message_t pair{};
    pair.type = MSG_PAIR;
    pair.device_id = 0; // master id
    pair.seq = 0;
    for (int i = 0; i < 20; ++i) {
      esp_now_send(broadcastAddress, (uint8_t*)&pair, sizeof(pair));
      delay(500);
    }
    Serial.println("Modo pareamento (MASTER) finalizado");
  } else {
    Serial.println("Aguardando registros (slaves se registrarão via broadcast)...");
  }
}

#endif

#ifdef ROLE_SLAVE
void setupSlave() {
  Serial.printf("ROLE: SLAVE id=%d\n", DEVICE_ID);
  // configure pins
  for (int i = 0; i < 5; ++i) pinMode(OUTPUT_PINS[i], OUTPUT);
  for (int i = 0; i < 2; ++i) pinMode(INPUT_PINS[i], INPUT);
  // BOOT pin to detect pairing confirmation
  pinMode(BOOT_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  esp_err_t r = esp_now_init();
  Serial.printf("esp_now_init=%d\n", r);
  esp_now_set_pmk((const uint8_t*)PMK);
  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  // Ensure broadcast peer exists (required on some firmwares)
  ensureBroadcastPeerAdded(false);

  // send registration to master via broadcast (normal boot)
  espnow_message_t reg{};
#ifdef DEVICE_ID
  reg.device_id = DEVICE_ID;
#else
  reg.device_id = 0;
#endif
  reg.type = MSG_REG;
  reg.seq = 0;
  reg.outputs = 0;
  reg.inputs = 0;
  esp_err_t s = esp_now_send(broadcastAddress, (uint8_t*)&reg, sizeof(reg));
  Serial.printf("Registration sent (broadcast) result=%d\n", s);
}
#endif

void setup() {
  Serial.begin(115200);
  delay(100);
  // common init
  WiFi.mode(WIFI_STA);

#ifdef ROLE_MASTER
  setupMaster();
#else
  setupSlave();
#endif

}

void loop() {
#ifdef ROLE_MASTER
  // Simple serial UI: send command -> format: s <id> <outputs>
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;
    if (line.charAt(0) == 's') {
      // parse
      int sp1 = line.indexOf(' ');
      int sp2 = line.indexOf(' ', sp1 + 1);
      if (sp1 > 0 && sp2 > sp1) {
        String idStr = line.substring(sp1 + 1, sp2);
        String outStr = line.substring(sp2 + 1);
        int id = idStr.toInt();
        int out = 0;
        if (outStr.startsWith("0x")) out = (int) strtol(outStr.c_str()+2, NULL, 16);
        else out = outStr.toInt();
        sendCommandToDevice(id, (uint8_t)out);
      } else {
        Serial.println("Formato: s <id> <outputs> (outputs decimal ou 0xHEX)");
      }
    }
  }
#else
  // slaves do not need a busy loop here; they react by callbacks
#if defined(ROLE_SLAVE)
  // Handle pairing confirmation: if a pairing request was received, user must press BOOT
  if (pairingRequested) {
    // timeout
    if (millis() - pairingRequestTs > PAIR_WINDOW_MS) {
      pairingRequested = false;
      Serial.println("Janela de pareamento expirou");
    } else {
      if (digitalRead(BOOT_PIN) == LOW) {
        // user confirmed pairing: send PAIR_ACK to requester
        espnow_message_t ack{};
        ack.type = MSG_PAIR_ACK;
#ifdef DEVICE_ID
        ack.device_id = DEVICE_ID;
#else
        ack.device_id = 0;
#endif
        ack.seq = 0;
        esp_err_t r = esp_now_send(pairingRequesterMac, (uint8_t*)&ack, sizeof(ack));
        Serial.printf("Enviado PAIR_ACK para "); printMac(pairingRequesterMac); Serial.printf(" result=%d\n", r);
        // add the requester as a peer (master) so future comms are encrypted
        // Currently master may have already added us; upgrade to encrypted peer as well
        addPeer(pairingRequesterMac, pairingRequesterDeviceId, true);
        pairingRequested = false;
      }
    }
  }
  // telemetry: if paired with master, send periodic status
  int masterIdx = findPeerIndexByDeviceId(0);
  if (masterIdx >= 0) {
    if (millis() - lastTelemetryTs >= TELEMETRY_INTERVAL_MS) {
      lastTelemetryTs = millis();
      // build status
      espnow_message_t st{};
      st.type = MSG_STATUS;
#ifdef DEVICE_ID
      st.device_id = DEVICE_ID;
#else
      st.device_id = 0;
#endif
      st.seq = seqCounter++;
      // read outputs state into mask
      uint8_t outmask = 0;
      for (int i = 0; i < 5; ++i) {
        int v = digitalRead(OUTPUT_PINS[i]);
        if (v) outmask |= (1 << i);
      }
      st.outputs = outmask;
      uint8_t inmask = 0;
      for (int i = 0; i < 2; ++i) {
        int v = digitalRead(INPUT_PINS[i]);
        if (v) inmask |= (1 << i);
      }
      st.inputs = inmask;
      esp_err_t r = esp_now_send(peers[masterIdx].mac, (uint8_t*)&st, sizeof(st));
      Serial.printf("Telemetry sent to master idx=%d result=%d\n", masterIdx, r);
    }
  }
  delay(200);
#else
  delay(1000);
#endif
#endif
}