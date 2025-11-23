#include <esp_now.h>
#include <WiFi.h>
#include "Arduino.h"

uint8_t Master[6] = {0x3C,0x8A,0x1F,0xBB,0x38,0x80};

#define PIN_VUP  2
#define PIN_VDN  22
#define PIN_TL   21
#define PIN_TU   19

#define PIN_IN1  0
#define PIN_IN2  35

// --- Configurar apelido deste módulo ---
const char *APELIDO = "PortaDE";

typedef struct {
  char apelido[10];
  bool cmdVUP;
  bool cmdVDN;
  bool cmdTL;
  bool cmdTU;
} comandoStruct;

typedef struct {
  char apelido[10];
  bool estadoIN1;
  bool estadoIN2;
} retornoStruct;

comandoStruct recebido;
retornoStruct enviar;

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  memcpy(&recebido, data, sizeof(recebido));

  digitalWrite(PIN_VUP, recebido.cmdVUP);
  digitalWrite(PIN_VDN, recebido.cmdVDN);
  digitalWrite(PIN_TL, recebido.cmdTL);
  digitalWrite(PIN_TU, recebido.cmdTU);
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  pinMode(PIN_VUP, OUTPUT);
  pinMode(PIN_VDN, OUTPUT);
  pinMode(PIN_TL, OUTPUT);
  pinMode(PIN_TU, OUTPUT);

  pinMode(PIN_IN1, INPUT_PULLUP);
  pinMode(PIN_IN2, INPUT_PULLUP);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

    esp_now_register_recv_cb(onReceive);
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, Master, 6);
    peer.encrypt = false;
    esp_now_add_peer(&peer);
    Serial.println("SLAVE PRONTO");
}

void loop() {
  enviar.estadoIN1 = digitalRead(PIN_IN1);
  enviar.estadoIN2 = digitalRead(PIN_IN2);
  strcpy(enviar.apelido, APELIDO);

  // Broadcast (master deve adicionar manualmente MAC do slave)
  esp_now_send(Master, (uint8_t *)&enviar, sizeof(enviar));

  delay(5000);
}
