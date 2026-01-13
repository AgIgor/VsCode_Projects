#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_err.h>
#include <Preferences.h>

// Pinos (ajuste se necessário)
const int BOOT_PIN = 0; // botão de boot (pressed = LOW em muitas placas)
const int LED_PIN = 2;  // LED de verificação nos slaves

// Pareamento
const uint32_t PAIR_TIMEOUT_MS = 10000;
const size_t MAX_PAYLOAD = 32;

enum MsgType : uint8_t {
  MSG_PAIR_REQUEST = 1,
  MSG_PAIR_ACCEPT = 2,
  MSG_PAIR_CONFIRM = 3,
  MSG_CMD_ALL = 4,
  MSG_CMD_UNICAST = 5,
  MSG_STATUS = 6
};

typedef struct __attribute__((packed)) {
  uint8_t type;
  uint8_t sender[6];
  uint8_t target[6];
  uint32_t nonce;
  uint8_t payload[MAX_PAYLOAD];
} esp_msg_t;

Preferences prefs;

uint8_t myMac[6];
uint8_t broadcastMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// Estado local
bool isPaired = false;
bool amMaster = false; // true se este nó for master para outro
uint8_t peerMac[6]; // MAC do par (master ou slave)

// PMK/LMK para criptografia (16 bytes)
const uint8_t LMK[16] = { 'S','e','g','u','r','a','n','c','a','E','s','p','N','o','w','!'};

// Temporizadores
unsigned long pairRequestStart = 0;
uint32_t lastNonce = 0;

// Helpers
void printMac(const uint8_t *mac) {
  for (int i=0;i<6;i++) {
    if (i) Serial.print(":");
    if (mac[i] < 16) Serial.print("0");
    Serial.print(mac[i], HEX);
  }
}

bool macEqual(const uint8_t *a, const uint8_t *b) {
  for (int i=0;i<6;i++) if (a[i]!=b[i]) return false;
  return true;
}

// adiciona peer; encrypt=true usa LMK
bool addPeer(const uint8_t *mac, bool encrypt=true) {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0; // current channel
  peerInfo.encrypt = encrypt ? 1 : 0;
  if (encrypt) memcpy(peerInfo.lmk, LMK, 16);
  if (esp_now_is_peer_exist(mac)) return true;
  esp_err_t res = esp_now_add_peer(&peerInfo);
  if (res == ESP_OK) {
    Serial.print("Peer adicionado: "); printMac(mac); Serial.println();
    return true;
  } else {
    Serial.print("Falha ao adicionar peer: "); Serial.println(esp_err_to_name(res));
    return false;
  }
}

void sendMessage(const uint8_t *destMac, const esp_msg_t &msg) {
  esp_err_t res = esp_now_send(destMac, (uint8_t*)&msg, sizeof(msg));
  if (res == ESP_OK) {
    return;
  }
  Serial.print("Erro envio esp-now: "); Serial.print(res); Serial.print(" ");
  Serial.println(esp_err_to_name(res));
}

// Callbacks
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Envio para "); printMac(mac_addr); Serial.print(" -> ");
  Serial.println(status==ESP_NOW_SEND_SUCCESS?"OK":"ERRO");
}

void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len < (int)sizeof(uint8_t)) return;
  esp_msg_t msg;
  memcpy(&msg, incomingData, min((int)sizeof(msg), len));

  if (msg.type == MSG_PAIR_REQUEST) {
    Serial.print("Recebido PAIR_REQUEST de "); printMac(msg.sender); Serial.println();
    // Se o usuário pressionar o botão (BOOT) dentro do tempo, aceita
    // Aqui consideramos que o usuário deverá apertar o botão para aceitar
    unsigned long start = millis();
    Serial.println("Pressione Boot para aceitar em 8s...");
    unsigned long deadline = start + 8000;
    while (millis() < deadline) {
      if (digitalRead(BOOT_PIN) == LOW) {
        // aceitar
        esp_msg_t reply = {};
        reply.type = MSG_PAIR_ACCEPT;
        memcpy(reply.sender, myMac, 6);
        memcpy(reply.target, msg.sender, 6);
        reply.nonce = msg.nonce;
        addPeer(msg.sender); // adiciona requester como slave
        // também adicione o peer localmente para possível comunicação futura
        addPeer(msg.sender);
        sendMessage(msg.sender, reply);
        // este nó passa a ser master do requester
        isPaired = true;
        amMaster = true;
        memcpy(peerMac, msg.sender, 6);
        Serial.print("Aceitei e sou MASTER de: "); printMac(peerMac); Serial.println();
        return;
      }
      delay(50);
    }
    Serial.println("Tempo para aceitar expirou.");
  }
  else if (msg.type == MSG_PAIR_ACCEPT) {
    // quem fez a solicitação deve receber accept
    if (macEqual(msg.target, myMac) && msg.nonce == lastNonce) {
      Serial.print("Recebido PAIR_ACCEPT de "); printMac(msg.sender); Serial.println();
      addPeer(msg.sender);
      // requester torna-se slave do accepter
      isPaired = true;
      amMaster = false;
      memcpy(peerMac, msg.sender, 6);
      // envia confirm
      esp_msg_t conf = {};
      conf.type = MSG_PAIR_CONFIRM;
      memcpy(conf.sender, myMac, 6);
      memcpy(conf.target, msg.sender, 6);
      conf.nonce = msg.nonce;
      sendMessage(msg.sender, conf);
      Serial.print("Pareado como SLAVE com: "); printMac(peerMac); Serial.println();
    }
  }
  else if (msg.type == MSG_PAIR_CONFIRM) {
    if (macEqual(msg.target, myMac) && msg.nonce == lastNonce) {
      Serial.print("Pair confirm recebido de "); printMac(msg.sender); Serial.println();
    }
  }
  else if (msg.type == MSG_CMD_ALL) {
    // comando broadcast para todos (ex.: togglear LED2)
    if (!amMaster) { // apenas slaves obedecem comando de master
      if (msg.payload[0] == 1) {
        digitalWrite(LED_PIN, HIGH);
      } else if (msg.payload[0] == 0) {
        digitalWrite(LED_PIN, LOW);
      }
    }
  }
}

void sendPairRequest() {
  esp_msg_t msg = {};
  msg.type = MSG_PAIR_REQUEST;
  memcpy(msg.sender, myMac, 6);
  memcpy(msg.target, broadcastMac, 6);
  lastNonce = esp_random();
  msg.nonce = lastNonce;
  Serial.println("Enviando PAIR_REQUEST broadcast...");
  esp_err_t res = esp_now_send(broadcastMac, (uint8_t*)&msg, sizeof(msg));
  if (res != ESP_OK) {
    Serial.print("Erro broadcast: "); Serial.print(res); Serial.print(" "); Serial.println(esp_err_to_name(res));
    // Tentativa de fallback: tentar adicionar peer de broadcast sem encriptação e reenviar
    Serial.println("Tentando adicionar peer broadcast sem criptografia e reenviar...");
    if (addPeer(broadcastMac, false)) {
      res = esp_now_send(broadcastMac, (uint8_t*)&msg, sizeof(msg));
      Serial.print("Reenvio resultado: "); Serial.print(res); Serial.print(" "); Serial.println(esp_err_to_name(res));
    }
  }
  pairRequestStart = millis();
}

void sendCmdToAll(uint8_t value) {
  esp_msg_t msg = {};
  msg.type = MSG_CMD_ALL;
  memcpy(msg.sender, myMac, 6);
  memcpy(msg.target, broadcastMac, 6);
  msg.payload[0] = value; // 1 = led on, 0 = led off
  esp_err_t res = esp_now_send(broadcastMac, (uint8_t*)&msg, sizeof(msg));
  if (res != ESP_OK) {
    Serial.print("Erro cmd all: "); Serial.print(res); Serial.print(" "); Serial.println(esp_err_to_name(res));
    if (addPeer(broadcastMac, false)) {
      res = esp_now_send(broadcastMac, (uint8_t*)&msg, sizeof(msg));
      Serial.print("Reenvio cmd all resultado: "); Serial.print(res); Serial.print(" "); Serial.println(esp_err_to_name(res));
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_err_t initRes = esp_now_init();
  if (initRes != ESP_OK) {
    Serial.print("ESP-NOW init failed: "); Serial.print(initRes); Serial.print(" ");
    Serial.println(esp_err_to_name(initRes));
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // set PMK (opcional) e LMK por peer
  esp_err_t pmkRes = esp_now_set_pmk((uint8_t*)"MySecretPMK12345");
  if (pmkRes != ESP_OK) {
    Serial.print("Warning: set_pmk failed: "); Serial.print(pmkRes); Serial.print(" ");
    Serial.println(esp_err_to_name(pmkRes));
  }

  WiFi.macAddress(myMac);
  Serial.print("MAC local: "); printMac(myMac); Serial.println();

  prefs.begin("espnow", false);
  // se houve um peer salvo, carregue (simples demo: salvamos apenas um par)
  if (prefs.isKey("peer0")) {
    String hex = prefs.getString("peer0");
    if (hex.length() == 12) {
      for (int i=0;i<6;i++) {
        String byteStr = hex.substring(i*2, i*2+2);
        peerMac[i] = (uint8_t) strtoul(byteStr.c_str(), NULL, 16);
      }
      addPeer(peerMac);
      isPaired = true;
      Serial.print("Peer salvo carregado: "); printMac(peerMac); Serial.println();
    }
  }

  Serial.println("Setup completo. Pressione Boot no dispositivo desejado para iniciar pareamento (ou segure aqui para enviar pedido).");
}

void loop() {
  // Se pressionar boot na inicialização, encaminha pedido de pareamento via broadcast
  static bool requestSent = false;
  if (!requestSent && digitalRead(BOOT_PIN) == LOW) {
    delay(50); // debounce
    if (digitalRead(BOOT_PIN) == LOW) {
      sendPairRequest();
      requestSent = true;
    }
  }

  // se estamos em modo pareamento (requester), aguardamos accept por um tempo
  if (pairRequestStart != 0 && !isPaired) {
    if (millis() - pairRequestStart > PAIR_TIMEOUT_MS) {
      Serial.println("Pareamento expirado, tente novamente.");
      pairRequestStart = 0;
      requestSent = false;
    }
  }

  // Se emparelhado e sou master, botão boot controla LED dos slaves (envia comando ALL)
  static bool lastBootState = HIGH;
  bool bootState = digitalRead(BOOT_PIN);
  if (isPaired && amMaster) {
    if (lastBootState == HIGH && bootState == LOW) {
      // botão pressionado -> alterna LED nos slaves
      Serial.println("Master: enviando comando ALL (toggle LED)");
      // para demo: liga
      sendCmdToAll(1);
    }
    if (lastBootState == LOW && bootState == HIGH) {
      // liberado -> apaga
      sendCmdToAll(0);
    }
  }
  lastBootState = bootState;

  // Periodicamente reporta status
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 5000) {
    lastStatus = millis();
    if (isPaired) {
      Serial.print("Paired com: "); printMac(peerMac); Serial.print(" role="); Serial.println(amMaster?"MASTER":"SLAVE");
    } else {
      Serial.println("Sem pareamento");
    }
  }
}