#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

typedef struct {
  int valor1;
  int valor2;
  bool estado;
} packet_t;

uint8_t gatewayMac[] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC};   // MAC do ESP32 B

void OnSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Status envio: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FALHA");
}

void setup() {
  Serial.begin(115200);
  Serial.println(WiFi.macAddress());

  WiFi.mode(WIFI_STA);
  Serial.print("MAC deste ESP: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, gatewayMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  esp_now_add_peer(&peerInfo);
}

void loop() {

  packet_t pacote;
  pacote.valor1 = analogRead(34);
  pacote.valor2 = random(0, 100);
  pacote.estado = digitalRead(0);

  esp_err_t result = esp_now_send(gatewayMac, (uint8_t *)&pacote, sizeof(pacote));

  if (result == ESP_OK)
    Serial.println("Pacote enviado");
  else
    Serial.println("Falha no envio");

  delay(2000);
}
