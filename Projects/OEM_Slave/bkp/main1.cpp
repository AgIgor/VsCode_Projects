/*
  ESP-NOW MASTER / SLAVE united file
  - Define ROLE_MASTER to compile as MASTER; comment it to compile as SLAVE.
  - Master: pareamento via broadcast; salva peers em NVS (Preferences).
  - Slave: responde ao pareamento; salva MAC do master em NVS; controla GPIOs reais via campo outputs.
*/

#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <esp_system.h>

////////////////////// CONFIG //////////////////////
// #define ROLE_MASTER    // <-- Descomente para MASTER. Comente para SLAVE.

#define BOOT_PIN 0            // botão BOOT (input pullup)
#define PAIR_HOLD_TIME 3000   // ms para segurar e entrar em pareamento
#define MAX_PEERS 20

// Mapeamento de outputs do SLAVE (usados quando slave recebe mode==0)
// bit 0 -> GPIO_OUT0, bit1 -> GPIO_OUT1, ...
#ifndef ROLE_MASTER
const uint8_t GPIO_OUT0 = 2;   // exemplo: LED ou relé no GPIO2
const uint8_t GPIO_OUT1 = 4;   // exemplo
const uint8_t GPIO_OUT2 = 16;  // exemplo
const uint8_t GPIO_OUT3 = 17;  // exemplo
#endif

///////////////////// STRUCT ////////////////////////
// Packed para evitar padding
typedef struct __attribute__((packed)) {
  uint8_t mode;     // 0=dados, 1=broadcast, 2=resp pareamento, 3=ack
  uint8_t mac[6];   // MAC do remetente
  bool bootState;
  uint8_t id;
  uint32_t contador;
  uint8_t outputs;  // bitmap de saídas (usado quando mode==0)
} Packet;

Packet txData;
Packet rxData;

///////////////////// VARS GLOBAIS //////////////////
uint8_t selfMac[6];
uint32_t contador = 0;
uint32_t lastSend = 0;
uint32_t lastBootTime = 0;
bool lastBootState = HIGH;
const uint16_t debounceTime = 40;
uint8_t broadcastAddr[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

Preferences prefs;

#ifdef ROLE_MASTER
// peer list in RAM and persisted in NVS
uint8_t peers[MAX_PEERS][6];
int peerCount = 0;
bool pairingMode = false;
uint32_t pairingStart = 0;

// NVS key
const char *PREF_NS = "espnow_master";
const char *PREF_KEY_PEERS = "peers_blob";
#endif

#ifndef ROLE_MASTER
// slave stores its master's mac
uint8_t masterMac[6] = {0,0,0,0,0,0};
bool awaitingAck = false;
const char *PREF_NS_SLAVE = "espnow_slave";
const char *PREF_KEY_MASTER = "master_mac";
#endif

///////////////////// HELPERS ///////////////////////

void printMAC(const uint8_t *mac){
  for (int i=0;i<6;i++){
    Serial.printf("%02X", mac[i]);
    if(i<5) Serial.print(":");
  }
}

bool macIsZero(const uint8_t *mac){
  for(int i=0;i<6;i++) if(mac[i]!=0) return false;
  return true;
}

///////////////////// NVS (MASTER) //////////////////
#ifdef ROLE_MASTER
// compact peers into blob: peerCount (1 byte) + peerCount*6 bytes
void savePeersToNVS(){
  prefs.begin(PREF_NS, false);
  size_t blobSize = 1 + peerCount * 6;
  uint8_t *blob = (uint8_t*)malloc(blobSize);
  if(!blob) { Serial.println("[MASTER] Falha malloc salvar peers"); prefs.end(); return; }
  blob[0] = (uint8_t)peerCount;
  for(int i=0;i<peerCount;i++) memcpy(&blob[1 + i*6], peers[i], 6);
  prefs.putBytes(PREF_KEY_PEERS, blob, blobSize);
  free(blob);
  prefs.end();
  Serial.printf("[MASTER] Salvo %d peers na NVS\n", peerCount);
}

void loadPeersFromNVS(){
  prefs.begin(PREF_NS, true);
  size_t sz = prefs.getBytesLength(PREF_KEY_PEERS);
  if(sz == 0){ Serial.println("[MASTER] Nenhum peer salvo"); prefs.end(); return; }
  uint8_t *blob = (uint8_t*)malloc(sz);
  if(!blob){ Serial.println("[MASTER] malloc fail carregar peers"); prefs.end(); return; }
  prefs.getBytes(PREF_KEY_PEERS, blob, sz);
  int cnt = blob[0];
  if(cnt > MAX_PEERS) cnt = MAX_PEERS;
  peerCount = 0;
  for(int i=0;i<cnt;i++){
    memcpy(peers[i], &blob[1 + i*6], 6);
    // tenta adicionar ao esp_now
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, peers[i], 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_err_t r = esp_now_add_peer(&peerInfo);
    if(r == ESP_OK || r == ESP_ERR_ESPNOW_EXIST){
      peerCount++;
      Serial.print("[MASTER] Peer carregado: ");
      printMAC(peers[i]);
      Serial.println();
    } else {
      Serial.printf("[MASTER] falha add_peer carregado: %d\n", r);
    }
  }
  free(blob);
  prefs.end();
}
#endif

///////////////////// NVS (SLAVE) //////////////////
#ifndef ROLE_MASTER
void saveMasterToNVS(){
  prefs.begin(PREF_NS_SLAVE, false);
  prefs.putBytes(PREF_KEY_MASTER, masterMac, 6);
  prefs.end();
  Serial.print("[SLAVE] Master salvo na NVS: ");
  printMAC(masterMac);
  Serial.println();
}

void loadMasterFromNVS(){
  prefs.begin(PREF_NS_SLAVE, true);
  size_t sz = prefs.getBytesLength(PREF_KEY_MASTER);
  if(sz == 6){
    prefs.getBytes(PREF_KEY_MASTER, masterMac, 6);
    Serial.print("[SLAVE] Master carregado: ");
    printMAC(masterMac);
    Serial.println();

    // adiciona ao esp_now
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, masterMac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_err_t r = esp_now_add_peer(&peer);
    if(r == ESP_OK || r == ESP_ERR_ESPNOW_EXIST){
      Serial.println("[SLAVE] Master adicionado como peer (NVS).");
    } else {
      Serial.printf("[SLAVE] falha add_peer master NVS: %d\n", r);
    }
  } else {
    Serial.println("[SLAVE] Nenhum master salvo na NVS.");
  }
  prefs.end();
}
#endif

///////////////////// ESP-NOW CALLBACK //////////////////
void onReceive(const uint8_t * mac, const uint8_t *incomingData, int len){
  // copia segura (evita overflow se pacotes estranhos)
  if(len != sizeof(Packet)){
    Serial.printf("[RX] Tamanho inesperado: %d (esperado %d)\n", len, (int)sizeof(Packet));
    return;
  }
  memcpy(&rxData, incomingData, sizeof(Packet));

#ifdef ROLE_MASTER
  // MASTER processa respostas de pareamento (mode==2) e pacotes mode==0
  if(rxData.mode == 2){
    Serial.print("[MASTER] Recebi resposta de pareamento de: ");
    printMAC(rxData.mac);
    Serial.println();

    // adiciona peer na esp_now
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, rxData.mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_err_t r = esp_now_add_peer(&peerInfo);
    if(r == ESP_OK || r == ESP_ERR_ESPNOW_EXIST){
      // adiciona na lista RAM se não existir
      bool exists = false;
      for(int i=0;i<peerCount;i++) if(memcmp(peers[i], rxData.mac, 6) == 0) { exists = true; break; }
      if(!exists){
        if(peerCount < MAX_PEERS){
          memcpy(peers[peerCount], rxData.mac, 6);
          peerCount++;
          Serial.print("[MASTER] Peer salvo em RAM: ");
          printMAC(rxData.mac);
          Serial.println();
          // salva tudo na NVS
          savePeersToNVS();
        } else {
          Serial.println("[MASTER] Lista de peers cheia, não foi salvo em RAM.");
        }
      } else {
        Serial.println("[MASTER] Peer já existia.");
      }

      // envia ACK final para confirmar pareamento e STOP pareamento
      Packet ack;
      ack.mode = 3;
      memcpy(ack.mac, selfMac, 6);
      ack.bootState = false;
      ack.id = 99;
      ack.contador = ++contador;
      ack.outputs = 0;
      esp_now_send(rxData.mac, (uint8_t*)&ack, sizeof(Packet));
      Serial.println("[MASTER] Enviado ACK final. Parando broadcast.");
      pairingMode = false;
    } else {
      Serial.printf("[MASTER] esp_now_add_peer falhou: %d\n", r);
    }
    return;
  }

  if(rxData.mode == 0){
    Serial.print("[MASTER] Recebeu dados de slave ");
    printMAC(rxData.mac);
    Serial.print(" boot=");
    Serial.print(rxData.bootState);
    Serial.print(" id=");
    Serial.print(rxData.id);
    Serial.print(" contador=");
    Serial.print(rxData.contador);
    Serial.print(" outputs=");
    Serial.println(rxData.outputs, BIN);
  }
#else
  // SLAVE processing
  if(rxData.mode == 1){
    Serial.println("[SLAVE] Broadcast recebido do master:");
    Serial.print("  master MAC: "); printMAC(rxData.mac); Serial.println();
    // adiciona master como peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, rxData.mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_err_t r = esp_now_add_peer(&peer);
    if(r == ESP_OK || r == ESP_ERR_ESPNOW_EXIST){
      // responde com meu MAC (PAIR_CONFIRM)
      Packet resp;
      resp.mode = 2;
      memcpy(resp.mac, selfMac, 6);
      resp.bootState = lastBootState;
      resp.id = 2;
      resp.contador = ++contador;
      resp.outputs = 0;
      esp_now_send(rxData.mac, (uint8_t*)&resp, sizeof(Packet));
      Serial.println("[SLAVE] Enviado PAIR_CONFIRM ao master.");
    } else {
      Serial.printf("[SLAVE] erro esp_now_add_peer broadcast: %d\n", r);
    }
    return;
  }

  if(rxData.mode == 3){
    // ACK do master; salvar MAC do master em NVS
    memcpy(masterMac, rxData.mac, 6);
    saveMasterToNVS();
    Serial.println("[SLAVE] Recebeu ACK do master. Pareamento OK.");
    awaitingAck = false;
    return;
  }

  if(rxData.mode == 0){
    Serial.println("[SLAVE] Recebeu comando do master:");
    Serial.print(" outputs bitmap = "); Serial.println(rxData.outputs, BIN);

    // controla GPIOs reais baseado em outputs bitmap
    digitalWrite(GPIO_OUT0, (rxData.outputs & 0x01) ? HIGH : LOW);
    digitalWrite(GPIO_OUT1, (rxData.outputs & 0x02) ? HIGH : LOW);
    digitalWrite(GPIO_OUT2, (rxData.outputs & 0x04) ? HIGH : LOW);
    digitalWrite(GPIO_OUT3, (rxData.outputs & 0x08) ? HIGH : LOW);

    // opcional: envia ack ou estado de volta ao master (aqui enviamos o bootState / status)
    Packet status;
    status.mode = 0;
    memcpy(status.mac, selfMac, 6);
    status.bootState = lastBootState;
    status.id = 2;
    status.contador = ++contador;
    status.outputs = rxData.outputs; // ecoa estado
    // envia para master (se conhecido)
    if(!macIsZero(masterMac)){
      esp_now_send(masterMac, (uint8_t*)&status, sizeof(Packet));
    }
    return;
  }

#endif
}

///////////////////// ENVIO BROADCAST (MASTER) //////////////////
#ifdef ROLE_MASTER
void sendBroadcast(){
  Packet pkt;
  pkt.mode = 1;
  memcpy(pkt.mac, selfMac, 6); // inclui MAC do master
  pkt.bootState = false;
  pkt.id = 99;
  pkt.contador = ++contador;
  pkt.outputs = 0;
  esp_err_t r = esp_now_send(broadcastAddr, (uint8_t*)&pkt, sizeof(Packet));
  if(r == ESP_OK) {
    Serial.println("[MASTER] Broadcast enviado.");
  } else {
    Serial.printf("[MASTER] Falha broadcast: %d\n", r);
  }
}
#endif

///////////////////// ENVIAR COMANDOS (MASTER) //////////////////
#ifdef ROLE_MASTER
// Envia comando mode==0 para todos peers na lista RAM
void sendCommandToPeers(uint8_t outputsBitmap){
  Packet cmd;
  cmd.mode = 0;
  memcpy(cmd.mac, selfMac, 6);
  cmd.bootState = false;
  cmd.id = 99;
  cmd.contador = ++contador;
  cmd.outputs = outputsBitmap;

  for(int i=0;i<peerCount;i++){
    esp_err_t r = esp_now_send(peers[i], (uint8_t*)&cmd, sizeof(Packet));
    if(r == ESP_OK){
      Serial.print("[MASTER] Comando enviado para ");
      printMAC(peers[i]);
      Serial.print(" outputs=");
      Serial.println(outputsBitmap, BIN);
    } else {
      Serial.printf("[MASTER] erro enviar comando para peer %d: %d\n", i, r);
    }
  }
}
#endif

///////////////////// SETUP ////////////////////////////
void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(100);

  // pega MAC
  esp_read_mac(selfMac, ESP_MAC_WIFI_STA);
  Serial.print("SELF MAC: "); printMAC(selfMac); Serial.println();

  // inicia ESP-NOW
  if(esp_now_init() != ESP_OK){
    Serial.println("Erro iniciar ESP-NOW");
    return;
  }

  // registra callback
  esp_now_register_recv_cb(onReceive);

  // adiciona broadcast peer (necessário para envio/recebimento broadcast em várias implementações)
  esp_now_peer_info_t pb = {};
  memcpy(pb.peer_addr, broadcastAddr, 6);
  pb.channel = 0;
  pb.encrypt = false;
  esp_err_t br = esp_now_add_peer(&pb);
  if(br == ESP_OK || br == ESP_ERR_ESPNOW_EXIST){
    Serial.println("Broadcast peer OK.");
  } else {
    Serial.printf("Falha adicionar broadcast peer: %d\n", br);
  }

#ifdef ROLE_MASTER
  // Carrega peers salvos (se existirem)
  prefs.begin(PREF_NS, true);
  prefs.end();
  loadPeersFromNVS();
  Serial.println("[MASTER] Setup concluido.");
#else
  // SLAVE: configura GPIOs de saída
  pinMode(GPIO_OUT0, OUTPUT);
  pinMode(GPIO_OUT1, OUTPUT);
  pinMode(GPIO_OUT2, OUTPUT);
  pinMode(GPIO_OUT3, OUTPUT);
  // garante estado inicial LOW
  digitalWrite(GPIO_OUT0, LOW);
  digitalWrite(GPIO_OUT1, LOW);
  digitalWrite(GPIO_OUT2, LOW);
  digitalWrite(GPIO_OUT3, LOW);

  // carregar master salvo (se houver)
  prefs.begin(PREF_NS_SLAVE, true);
  prefs.end();
  loadMasterFromNVS();

  Serial.println("[SLAVE] Setup concluido.");
#endif
}

///////////////////// LOOP ////////////////////////////
void loop(){
  bool leitura = digitalRead(BOOT_PIN);
  if(millis() - lastBootTime > debounceTime){
    if(leitura != lastBootState){
      lastBootState = leitura;
      lastBootTime = millis();
#ifdef ROLE_MASTER
      if(lastBootState == LOW){
        pairingStart = millis();
      }
#else
      // SLAVE: se já tiver master conhecido, envia mudança de boot para o master
      if(!macIsZero(masterMac)){
        // envia um pacote de status (mode 0) para o master com bootState
        Packet p;
        p.mode = 0;
        memcpy(p.mac, selfMac, 6);
        p.bootState = lastBootState;
        p.id = 2;
        p.contador = ++contador;
        p.outputs = 0;
        esp_now_send(masterMac, (uint8_t*)&p, sizeof(Packet));
        Serial.print("[SLAVE] Mudança BOOT enviada ao master: ");
        Serial.println(lastBootState);
      } else {
        Serial.println("[SLAVE] Mudança BOOT detectada, mas sem master conhecido.");
      }
#endif
    }
  }

#ifdef ROLE_MASTER
  // entrar em modo pareamento ao manter BOOT por X ms
  if(!pairingMode && lastBootState == LOW && (millis() - pairingStart > PAIR_HOLD_TIME)){
    pairingMode = true;
    Serial.println("[MASTER] MODO PAREAMENTO ATIVADO!");
  }

  // enquanto em pareamento, envia broadcast periódico
  if(pairingMode && millis() - lastSend > 1000){
    lastSend = millis();
    sendBroadcast();
  }

  // se NÃO em pareamento, envia comandos periódicos para peers
  static uint8_t pattern = 0;
  if(!pairingMode && millis() - lastSend > 1500){
    lastSend = millis();
    // exemplo de padrão rotativo: alterna bits para demonstrar controle de saídas
    pattern = (pattern + 1) & 0x0F; // usa 4 bits
    sendCommandToPeers(pattern);
  }
#endif

  delay(10);
}
