#include <esp_now.h>
#include <WiFi.h>

uint8_t NODE_A[] = {0x58,0xBF,0x25,0x81,0x3E,0x98};  // MAC do outro ESP32

bool last = HIGH, flag = false;
#define bot 0
#define led 2

typedef struct {
  int valor;
  bool estado;
} Packet;

void OnSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Status envio: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FALHOU");
}
void OnReceive(const uint8_t *mac, const uint8_t *data, int len) {
  Packet p;
  memcpy(&p, data, sizeof(p));

  Serial.print("De: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  Serial.print("Valor: ");
  Serial.println(p.valor);

  Serial.print("Estado: ");
  Serial.println(p.estado ? "true" : "false");
  digitalWrite(led, p.estado);

  Serial.println("------------------");
}

void setup() {
  Serial.begin(115200);
  pinMode(bot, INPUT_PULLUP);
  pinMode(led, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnSent);
  esp_now_register_recv_cb(OnReceive);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, NODE_A, 6);
  peerInfo.channel = 0; 
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Erro ao adicionar peer");
  }
  Serial.println(WiFi.macAddress());
}

Packet p;

void loop() {

  bool cur = digitalRead(bot);

  if (last == HIGH && cur == LOW) flag = true;         // apertou
  if (last == LOW && cur == HIGH && flag) {            // soltou

    flag = false;
    p.valor = millis();
    p.estado = !p.estado;
  
    esp_now_send(NODE_A, (uint8_t*)&p, sizeof(p));
  }

  last = cur;

  delay(50);
}
