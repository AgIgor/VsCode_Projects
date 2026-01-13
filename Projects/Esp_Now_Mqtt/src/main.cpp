#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_now.h>

// ---------- CONFIG WIFI + MQTT ------------
const char* ssid     = "VIVOFIBRA-79D0";
const char* password = "58331BB245";
const char* mqtt_server = "192.168.15.51";  // IP do broker MQTT
int mqtt_port = 1883;

// ---------- STRUCT DOS DADOS ------------
typedef struct {
  int valor1;
  int valor2;
  bool estado;
} packet_t;

// ---------- GLOBALS ----------
WiFiClient espClient;
PubSubClient client(espClient);

// ---------- ESP-NOW CALLBACK ----------
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  packet_t pacote;
  memcpy(&pacote, incomingData, sizeof(pacote));

  Serial.println("Recebido via ESP-NOW:");
  Serial.printf("valor1: %d | valor2: %d | estado: %d\n",
                pacote.valor1, pacote.valor2, pacote.estado);

  // --- PUBLICAR NO MQTT ---
  char payload[60];
  sprintf(payload, "{\"v1\":%d,\"v2\":%d,\"estado\":%d}",
          pacote.valor1, pacote.valor2, pacote.estado);

  client.publish("esp32/gateway/dados", payload);
}

// ---------- MQTT RECONNECT ----------
void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando MQTT...");
    if (client.connect("ESP32-Gateway"), "agigor", "1207igor") {
      Serial.println("Conectado!");
      delay(1000);
    } else {
      Serial.print("Falhou (");
      Serial.print(client.state());
      Serial.println("). Tentando em 2s");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // ---------- WIFI STA (necessário para MQTT) ----------
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println(WiFi.macAddress());
  Serial.print("Conectando Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println(" OK!");

  client.setServer(mqtt_server, mqtt_port);

  // ---------- ESP-NOW ----------
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  // Adiciona qualquer ESP sem MAC fixo:
  esp_now_peer_info_t peerInfo = {};
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  uint8_t broadcastAddress[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  esp_now_add_peer(&peerInfo);
}

void loop() {
  if (!client.connected()){
    reconnect();
  }
  client.loop();
  delay(10);
} 
