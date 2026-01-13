#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>

Preferences prefs;

#define LED_PIN 2
#define BTN_PIN 0

struct Packet {
  char cmd[16];
  int value;
};

bool paired = false;
uint8_t masterMac[6];
int myID = -1;
char myAlias[16] = {0};

// -----------------------------------------------------
// SALVAR CONFIGURAÇÃO
// -----------------------------------------------------
void saveConfig() {
  prefs.putBool("paired", true);
  prefs.putBytes("master", masterMac, 6);
  prefs.putInt("id", myID);
  prefs.putString("alias", myAlias);
}

// -----------------------------------------------------
// CARREGAR CONFIG
// -----------------------------------------------------
void loadConfig() {
  paired = prefs.getBool("paired", false);

  if (paired) {
    prefs.getBytes("master", masterMac, 6);
    myID = prefs.getInt("id", -1);

    String alias = prefs.getString("alias", "");
    alias.toCharArray(myAlias, 16);
  }
}

void sendHelloBroadcast() {
  Packet pkt;
  strcpy(pkt.cmd, "HELLO");
  pkt.value = 0;

  uint8_t broadcastAddr[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  esp_now_send(broadcastAddr, (uint8_t*)&pkt, sizeof(pkt));
}

void sendButtonState() {
  Packet pkt;
  strcpy(pkt.cmd, "BTN");
  pkt.value = digitalRead(BTN_PIN);

  esp_now_send(masterMac, (uint8_t*)&pkt, sizeof(pkt));
}

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  Packet pkt;
  memcpy(&pkt, data, sizeof(pkt));

  if (strcmp(pkt.cmd, "ACK") == 0) {
    // Master confirmou pareamento
    memcpy(masterMac, mac, 6);
    Serial.println("ACK recebido — pareamento confirmado!");
    return;
  }

  if (strcmp(pkt.cmd, "CFG") == 0) {
    // Recebeu ID — salva na NVS
    myID = pkt.value;
    sprintf(myAlias, "SLAVE_%02d", myID + 1);

    paired = true;
    saveConfig();

    Serial.printf("Config recebida: ID=%d Alias=%s\n", myID, myAlias);
    return;
  }

  if (strcmp(pkt.cmd, "LED") == 0) {
    digitalWrite(LED_PIN, pkt.value ? HIGH : LOW);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  prefs.begin("slave", false);
  loadConfig();

  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);

  // Inicializa ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onReceive);

  // Se já for pareado, re-adicionar o master como peer
  if (paired) {
    esp_now_peer_info_t p = {};
    memcpy(p.peer_addr, masterMac, 6);
    p.channel = 0;
    p.encrypt = false;
    esp_now_add_peer(&p);

    Serial.printf("Pareado como %s (ID=%d)\n", myAlias, myID);
  } 
  else {
    Serial.println("SLAVE não pareado — enviando HELLO...");
  }
}

unsigned long lastHello = 0;
unsigned long lastBtnSend = 0;
int lastBtn = 1;

void loop() {

  // ---------------------------------------------------------
  // ENVIAR HELLO ENQUANTO NÃO PAREADO
  // ---------------------------------------------------------
  if (!paired) {
    if (millis() - lastHello > 2000) {
      sendHelloBroadcast();
      lastHello = millis();
    }
    return;
  }

  // ---------------------------------------------------------
  // LEITURA DO BOTÃO + ENVIO
  // ---------------------------------------------------------
  int btn = digitalRead(BTN_PIN);
  if (btn != lastBtn && millis() - lastBtnSend > 70) {
    sendButtonState();
    lastBtnSend = millis();
    lastBtn = btn;
  }
}
