#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>

Preferences prefs;

struct PeerInfo {
  uint8_t mac[6];
  char alias[16];
};

#define MAX_PEERS 20
PeerInfo peers[MAX_PEERS];
int peerCount = 0;

struct Packet {
  char cmd[16];
  int value;
};

void addPeerIfNeeded(uint8_t *mac) {
  // Verificar se já existe
  for (int i = 0; i < peerCount; i++) {
    if (memcmp(peers[i].mac, mac, 6) == 0) {
      return; // já existe
    }
  }

  // Novo — adicionar
  if (peerCount < MAX_PEERS) {
    memcpy(peers[peerCount].mac, mac, 6);
    sprintf(peers[peerCount].alias, "SLAVE_%02d", peerCount + 1); // gera apelido automático
    peerCount++;

    // Salvar na NVS
    prefs.putBytes("peers", peers, sizeof(peers));
    prefs.putInt("count", peerCount);

    // Adicionar ao ESP-NOW
    esp_now_peer_info_t p = {};
    memcpy(p.peer_addr, mac, 6);
    p.channel = 0;
    p.encrypt = false;
    esp_now_add_peer(&p);

    Serial.println("Novo peer adicionado!");
  }
}

void sendAck(uint8_t *mac) {
  Packet pkg;
  strcpy(pkg.cmd, "ACK");
  pkg.value = 1;

  esp_now_send(mac, (uint8_t*)&pkg, sizeof(pkg));
}

void sendConfig(uint8_t *mac) {
  Packet pkg;
  strcpy(pkg.cmd, "CFG");
  
  // Descobrir o alias deste MAC
  for (int i = 0; i < peerCount; i++) {
    if (memcmp(peers[i].mac, mac, 6) == 0) {
      pkg.value = i; // envia ID do slave
      break;
    }
  }

  esp_now_send(mac, (uint8_t*)&pkg, sizeof(pkg));
}

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  Packet pkt;
  memcpy(&pkt, data, sizeof(pkt));

  if (strcmp(pkt.cmd, "HELLO") == 0) {
    Serial.println("Novo SLAVE detectado!");

    addPeerIfNeeded((uint8_t*)mac);
    sendAck((uint8_t*)mac);
    sendConfig((uint8_t*)mac);
  }

  if (strcmp(pkt.cmd, "BTN") == 0) {
    Serial.print("Botão do SLAVE: ");
    Serial.println(pkt.value);
  }
}

void listPeers() {
  Serial.println("\n--- PEERS REGISTRADOS ---");
  for (int i = 0; i < peerCount; i++) {
    Serial.printf("[%d] %02X:%02X:%02X:%02X:%02X:%02X  Alias: %s\n",
      i,
      peers[i].mac[0], peers[i].mac[1], peers[i].mac[2],
      peers[i].mac[3], peers[i].mac[4], peers[i].mac[5],
      peers[i].alias
    );
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  prefs.begin("espnow", false);
  peerCount = prefs.getInt("count", 0);
  prefs.getBytes("peers", peers, sizeof(peers));

  esp_now_init();
  esp_now_register_recv_cb(onReceive);

  // Recarregar peers na inicialização
  for (int i = 0; i < peerCount; i++) {
    esp_now_peer_info_t p = {};
    memcpy(p.peer_addr, peers[i].mac, 6);
    p.channel = 0;
    p.encrypt = false;
    esp_now_add_peer(&p);
  }

  listPeers();
}

void loop() {
  // Exemplo para testar: enviar comando aos slaves
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');

    if (cmd.startsWith("led ")) {
      int id = cmd.substring(4).toInt();

      if (id >= 0 && id < peerCount) {
        Packet pkg;
        strcpy(pkg.cmd, "LED");
        pkg.value = 1;

        esp_now_send(peers[id].mac, (uint8_t*)&pkg, sizeof(pkg));
        Serial.println("Comando enviado!");
      }
    }

    if (cmd == "list") {
      listPeers();
    }
  }
}
