#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Exemplo de comunicação estilo I2C sem fio usando ESP-NOW (ESP32)
// - Permite escolher papel: mestre ou escravo via Serial no boot
// - Mestre tem lista de peers (MAC -> node_id) e pode enviar dados individuais
// - Escravo tem node_id e processa mensagens destinadas a ele

// Ajuste estes valores conforme sua rede
#define MAX_PAYLOAD 200

// Comandos de controle
#define CMD_PING  1
#define CMD_PONG  2

// Intervalo de ping em ms
#define PING_INTERVAL 5000

// Comando para estado do botão
#define CMD_BTN_STATE 3

// Pino do botão BOOT (devkit comum usa GPIO0)
#define BOOT_BUTTON_PIN 0

// Debounce do botão em ms
#define BTN_DEBOUNCE_MS 50

typedef struct __attribute__((packed)) {
  uint8_t dest_id;   // ID do nó destino
  uint8_t src_id;    // ID do nó origem
  uint16_t seq;      // sequência
  uint8_t cmd;       // código de comando / tipo
  uint8_t len;       // comprimento útil do payload
  uint8_t payload[MAX_PAYLOAD];
} espnow_message_t;

// --- Configuração inicial ---
bool isMaster = false;
uint8_t nodeId = 0; // alterado em modo escravo

// Lista de peers conhecida pelo mestre
struct Peer { uint8_t mac[6]; uint8_t id; };
// Preencha com os endereços MAC reais dos seus nós e IDs correspondentes
Peer peers[] = {
  // Exemplo: { {0x24,0x6F,0x28,0xXX,0xXX,0xXX}, 1 },
  { {0x3C,0x8A,0x1F,0xBB,0x38,0x80}, 1 },
  { {0x58,0xBF,0x25,0x81,0x3E,0x98}, 2 },
  { {0x32,0x1A,0x1F,0xBB,0x32,0x80}, 3 },
  { {0x32,0x5A,0x1F,0x3B,0x32,0x30}, 4 },
};
size_t peersCount = sizeof(peers)/sizeof(peers[0]);

// MAC do mestre (se quiser que escravos respondam ao mestre, configure aqui)
uint8_t masterMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; // substituir no escravo

volatile uint16_t txSeq = 0;
unsigned long lastPingMillis = 0;
// Estado do botão (escravo)
int lastButtonRaw = HIGH;
bool lastButtonStable = false;
unsigned long lastButtonDebounceMillis = 0;

// --- Helpers ---
String macToString(const uint8_t *mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  return String(buf);
}

void printHex(const uint8_t *data, size_t len) {
  for (size_t i=0;i<len;i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    if (i+1 < len) Serial.print(':');
  }
}

// --- Funções principais ---


bool addPeerMac(const uint8_t *mac) {
  esp_now_peer_info_t pi = {};
  memcpy(pi.peer_addr, mac, 6);
  pi.channel = 0;
  pi.encrypt = 0;
  if (esp_now_is_peer_exist(mac)) return true;
  esp_err_t r = esp_now_add_peer(&pi);
  return r == ESP_OK || r == ESP_ERR_ESPNOW_EXIST;
}

bool sendToMac(const uint8_t *mac, espnow_message_t &msg) {
  esp_err_t r = esp_now_send(mac, (uint8_t*)&msg, sizeof(espnow_message_t));
  return r == ESP_OK;
}

bool sendToNodeId(uint8_t destId, const uint8_t *mac, const uint8_t *data, uint8_t len, uint8_t cmd=0) {
  if (len > MAX_PAYLOAD) return false;
  espnow_message_t m = {};
  m.dest_id = destId;
  m.src_id = nodeId;
  m.seq = txSeq++;
  m.cmd = cmd;
  m.len = len;
  if (len) memcpy(m.payload, data, len);
  return sendToMac(mac, m);
}

// Envia um ping contendo o timestamp (4 bytes) no payload
void sendPingToMac(uint8_t destId, const uint8_t *mac) {
  uint32_t ts = (uint32_t)millis();
  uint8_t payload[4];
  payload[0] = ts & 0xFF;
  payload[1] = (ts >> 8) & 0xFF;
  payload[2] = (ts >> 16) & 0xFF;
  payload[3] = (ts >> 24) & 0xFF;
  if (sendToNodeId(destId, mac, payload, 4, CMD_PING)) {
    Serial.printf("Ping sent to id=%d mac=%s ts=%u\n", destId, macToString(mac).c_str(), ts);
  } else {
    Serial.printf("Falha ao enviar ping para id=%d mac=%s\n", destId, macToString(mac).c_str());
  }
}

// Envia estado do botão ao mestre (payload: 1 = pressed, 0 = released)
void sendButtonStateToMaster(bool pressed) {
  uint8_t payload[1];
  payload[0] = pressed ? 1 : 0;
  if (sendToNodeId(0, masterMac, payload, 1, CMD_BTN_STATE)) {
    Serial.printf("Enviado estado do botao para master: %s\n", pressed?"PRESSED":"RELEASED");
  } else {
    Serial.println("Falha ao enviar estado do botao ao master");
  }
}

// --- Callbacks ---
void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  // Recebido no escravo (ou em master escutando)
  espnow_message_t msg;
  if ((size_t)len < sizeof(espnow_message_t) - MAX_PAYLOAD) return; // inválido
  memcpy(&msg, incomingData, min((int)sizeof(msg), len));

  if (msg.dest_id != nodeId && msg.dest_id != 0xFF) {
    // Mensagem não é para este nó (descartar)
    return;
  }

  Serial.printf("Recebido from %s -> dest_id=%d src_id=%d seq=%u cmd=%u len=%u\n",
                macToString(mac).c_str(), msg.dest_id, msg.src_id, msg.seq, msg.cmd, msg.len);
  if (msg.len > 0 && msg.len <= MAX_PAYLOAD) {
    Serial.print("Payload: ");
    for (uint8_t i=0;i<msg.len;i++) Serial.write(msg.payload[i]);
    Serial.println();
  }

  // Aqui você pode implementar respostas, alterações de estado, etc.
  // Tratamento de PING/PONG
  if (msg.cmd == CMD_PING) {
    // responder com PONG (eco do payload)
    espnow_message_t reply = {};
    reply.dest_id = msg.src_id;
    reply.src_id = nodeId;
    reply.seq = txSeq++;
    reply.cmd = CMD_PONG;
    reply.len = msg.len;
    if (msg.len) memcpy(reply.payload, msg.payload, msg.len);
    sendToMac(mac, reply);
    Serial.printf("Respondendo PONG para src_id=%d mac=%s\n", msg.src_id, macToString(mac).c_str());
    return;
  }

  if (msg.cmd == CMD_PONG) {
    // calcular RTT se payload contiver timestamp (4 bytes)
    if (msg.len >= 4) {
      uint32_t ts = (uint32_t)msg.payload[0] | ((uint32_t)msg.payload[1]<<8) | ((uint32_t)msg.payload[2]<<16) | ((uint32_t)msg.payload[3]<<24);
      uint32_t now = (uint32_t)millis();
      uint32_t rtt = now - ts;
      Serial.printf("PONG recebido de src_id=%d mac=%s RTT=%ums\n", msg.src_id, macToString(mac).c_str(), rtt);
    } else {
      Serial.printf("PONG recebido de src_id=%d mac=%s\n", msg.src_id, macToString(mac).c_str());
    }
    return;
  }

  if (msg.cmd == CMD_BTN_STATE) {
    if (msg.len >= 1) {
      uint8_t st = msg.payload[0];
      Serial.printf("Button state from src_id=%d mac=%s -> %s\n", msg.src_id, macToString(mac).c_str(), st?"PRESSED":"RELEASED");
    } else {
      Serial.printf("Button state from src_id=%d mac=%s (no payload)\n", msg.src_id, macToString(mac).c_str());
    }
    return;
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Envio para %s: %s\n", macToString(mac_addr).c_str(), status==ESP_NOW_SEND_SUCCESS?"OK":"FAIL");
}


bool initEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro iniciando ESP-NOW");
    return false;
  }
  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);
  return true;
}

// --- Setup / Loop ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP-NOW I2C-like example");

  // Escolha de papel pelo usuário
  Serial.println("Digite 'm' para MASTER ou 's' para SLAVE e pressione ENTER:");
  while (!Serial.available()) delay(10);
  char c = Serial.read();
  if (c == 'm' || c == 'M') {
    isMaster = true;
    Serial.println("Modo: MASTER");
    // NodeId do mestre por convenção 0
    nodeId = 0;
  } else {
    isMaster = false;
    Serial.println("Modo: SLAVE");
    Serial.println("Informe um ID numérico para este nó (1-254) e pressione ENTER:");
    while (!Serial.available()) delay(10);
    nodeId = (uint8_t)atoi(Serial.readStringUntil('\n').c_str());
    Serial.printf("Node ID configurado: %d\n", nodeId);
    Serial.println("Se desejar que este escravo responda ao mestre, informe o MAC do mestre no formato AA:BB:CC:DD:EE:FF e ENTER:");
    String macs = Serial.readStringUntil('\n');
    if (macs.length()) {
      uint8_t tmp[6];
      if (sscanf(macs.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                 &tmp[0],&tmp[1],&tmp[2],&tmp[3],&tmp[4],&tmp[5]) == 6) {
        memcpy(masterMac, tmp, 6);
        addPeerMac(masterMac);
        Serial.printf("Master MAC set to %s\n", macToString(masterMac).c_str());
      }
    }
  }

  if (!initEspNow()) while(true) { delay(1000); }

  if (isMaster) {
    // Adicione peers configurados
    for (size_t i=0;i<peersCount;i++) {
      addPeerMac(peers[i].mac);
      Serial.printf("Peer %d -> %s\n", peers[i].id, macToString(peers[i].mac).c_str());
    }
    Serial.println("Comandos (master):\n  send <node_id> <mensagem>\n  list\n");
  } else {
    Serial.printf("Escravo pronto (ID=%d). Aguardando mensagens...\n", nodeId);
    // Inicializa pino do botão e estados para debounce
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    lastButtonRaw = digitalRead(BOOT_BUTTON_PIN);
    lastButtonStable = (lastButtonRaw == LOW);
    lastButtonDebounceMillis = millis();
  }

  Serial.println(WiFi.macAddress());
}

void loop() {
  if (isMaster) {
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) return;
      if (line.equalsIgnoreCase("list")) {
        Serial.println("Peers:");
        for (size_t i=0;i<peersCount;i++) {
          Serial.printf(" id=%d mac=%s\n", peers[i].id, macToString(peers[i].mac).c_str());
        }
        return;
      }
      if (line.startsWith("send ")) {
        int sp = line.indexOf(' ', 5);
        String idStr = line.substring(5, sp>0?sp:line.length());
        uint8_t dest = (uint8_t)atoi(idStr.c_str());
        String msg = (sp>0) ? line.substring(sp+1) : String();
        // find peer
        bool found=false;
        for (size_t i=0;i<peersCount;i++) {
          if (peers[i].id == dest) {
            sendToNodeId(dest, peers[i].mac, (const uint8_t*)msg.c_str(), msg.length(), 0);
            Serial.printf("Enviando para id=%d mac=%s mensagem='%s'\n", dest, macToString(peers[i].mac).c_str(), msg.c_str());
            found=true; break;
          }
        }
        if (!found) Serial.println("Peer não encontrado. Atualize a lista 'peers' no código.");
      }
    }
    // Envio periódico de PING para todos os peers
    unsigned long now = millis();
    if (now - lastPingMillis >= PING_INTERVAL) {
      lastPingMillis = now;
      for (size_t i = 0; i < peersCount; i++) {
        sendPingToMac(peers[i].id, peers[i].mac);
      }
    }
  } else {
    // Escravo: se masterMac estiver configurado, enviar pings periódicos ao mestre
    unsigned long now = millis();

    // Leitura do botão com debounce (não-bloqueante)
    int raw = digitalRead(BOOT_BUTTON_PIN);
    if (raw != lastButtonRaw) {
      // mudança detectada, reinicia debounce
      lastButtonDebounceMillis = now;
      lastButtonRaw = raw;
    }
    if ((now - lastButtonDebounceMillis) > BTN_DEBOUNCE_MS) {
      bool stablePressed = (lastButtonRaw == LOW);
      if (stablePressed != lastButtonStable) {
        lastButtonStable = stablePressed;
        // enviar atualização para o mestre se configurado
        bool masterSet = !(masterMac[0]==0xFF && masterMac[1]==0xFF && masterMac[2]==0xFF && masterMac[3]==0xFF && masterMac[4]==0xFF && masterMac[5]==0xFF);
        if (masterSet) sendButtonStateToMaster(stablePressed);
      }
    }

    // Envio periódico de PING ao mestre (se configurado)
    if (now - lastPingMillis >= PING_INTERVAL) {
      lastPingMillis = now;
      bool masterSet = !(masterMac[0]==0xFF && masterMac[1]==0xFF && masterMac[2]==0xFF && masterMac[3]==0xFF && masterMac[4]==0xFF && masterMac[5]==0xFF);
      if (masterSet) {
        sendPingToMac(0, masterMac);
      }
    }
    delay(20);
  }
}
