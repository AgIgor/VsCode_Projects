#include <WiFi.h>
#include <esp_now.h>

#define bot_pin 0
#define led_pin 2

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct {
  char msg[20];
} Packet;

Packet pacote;

// ====== Função para verificar se peer já está adicionado ======
bool peerExiste(const uint8_t *mac) {
  esp_now_peer_info_t peer;
  return esp_now_get_peer(mac, &peer) == ESP_OK;
}

// ====== Função para adicionar peer dinamicamente ======
bool adicionaPeer(const uint8_t *mac) {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (!peerExiste(mac)) {
    Serial.print("Adicionando peer: ");
    for(int i=0; i<6; i++){ Serial.printf("%02X:", mac[i]); }
    Serial.println();

    return esp_now_add_peer(&peerInfo) == ESP_OK;
  }
  return true; // já existia
}

// ====== Função para enviar confirmação ======
void enviarConfirmacao(const uint8_t *mac) {
  Packet resposta;
  strcpy(resposta.msg, "OK");

  esp_err_t status = esp_now_send(mac, (uint8_t*)&resposta, sizeof(resposta));

  Serial.print("Confirmacao enviada: ");
  Serial.println(status == ESP_OK ? "SUCESSO" : "FALHA");
}

// ====== Callback de recebimento ======
void OnRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&pacote, incomingData, sizeof(pacote));

  Serial.print("Recebido: ");
  Serial.println(pacote.msg);

  Serial.print("MAC do remetente: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  if(strcmp(pacote.msg, "pair")==0){
    Serial.println("pair mode...");

    // Salvar MAC se não estiver pareado
    if (!peerExiste(mac)) {
      Serial.println("Novo dispositivo encontrado, salvando...");
      if (adicionaPeer(mac)) {
        Serial.println("Peer adicionado!");
      } else {
        Serial.println("Falha ao adicionar peer!");
        return;
      }
    }
  
    // Enviar confirmação
    while(digitalRead(bot_pin)){
      Serial.println("Aguardando botão");
      delay(1000);
    }
  
    enviarConfirmacao(mac);
  }
}

void OnSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Status do envio: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCESSO" : "FALHA");
}

void setup() {
  Serial.begin(115200);
  pinMode(led_pin, OUTPUT);
  pinMode(bot_pin, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnRecv);
  esp_now_register_send_cb(OnSent);

  // 🔥 Necessário para conseguir broadcast
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Falha ao adicionar peer broadcast");
  }
}

void loop() {
  delay(10);
}
