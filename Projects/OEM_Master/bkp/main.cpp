#include <esp_now.h>
#include <WiFi.h>

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

comandoStruct enviar;
retornoStruct recebido;

// LISTA DE SLAVES
typedef struct {
  const char *apelido;
  uint8_t mac[6];
} dispositivo;

dispositivo slaves[] = {
  {"PortaDE", {0x58,0xBF,0x25,0x81,0x3E,0x98}},
  {"PortaDD", {0x24, 0x6F, 0x28, 0xAA, 0x11, 0x02}},
  {"PortaTE", {0x24, 0x6F, 0x28, 0xAA, 0x11, 0x03}},
  {"PortaTD", {0x24, 0x6F, 0x28, 0xAA, 0x11, 0x04}}
};
int qtdSlaves = sizeof(slaves) / sizeof(slaves[0]);

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  memcpy(&recebido, data, sizeof(recebido));
  Serial.printf("[RETORNO] %s | IN1=%d | IN2=%d\n", recebido.apelido, recebido.estadoIN1, recebido.estadoIN2);
}

void sendTo(const char *apelido) {
  for (int i = 0; i < qtdSlaves; i++) {
    if (strcmp(apelido, slaves[i].apelido) == 0) {
      strcpy(enviar.apelido, apelido);
      esp_now_send(slaves[i].mac, (uint8_t *)&enviar, sizeof(enviar));
      return;
    }
  }
  Serial.println("Apelido não encontrado!");
}

void processaComando(String linha) {
  linha.trim();
  int espaco = linha.indexOf(' ');
  if (espaco < 0) return;

  String alvo = linha.substring(0, espaco);
  String comando = linha.substring(espaco + 1);

  enviar.cmdVUP = (comando == "VUP");
  enviar.cmdVDN = (comando == "VDN");
  enviar.cmdTL  = (comando == "TL");
  enviar.cmdTU  = (comando == "TU");

  sendTo(alvo.c_str());
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onReceive);

  // Registrar peers
  for (int i = 0; i < qtdSlaves; i++) {
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, slaves[i].mac, 6);
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }

  Serial.println("MASTER PRONTO");
  Serial.println("Exemplo: PortaTD VUP");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    processaComando(line);
  }
}
