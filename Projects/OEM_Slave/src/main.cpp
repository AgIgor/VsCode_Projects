#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#define bot_pin 0
#define led_pin 2

// broadcast para descoberta inicial (apenas para descoberta; não é encriptado)
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- Segurança ESP-NOW ---
// Defina aqui sua chave mestre de 16 bytes (PMK) e a chave local (LMK) usada para peers.
// Troque os bytes abaixo por um valor seguro gerado aleatoriamente antes de usar em produção.
const uint8_t PMK[16] = { 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08, 0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10 };
const uint8_t LMK[16] = { 0x10,0x0F,0x0E,0x0D,0x0C,0x0B,0x0A,0x09, 0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01 };

// Lista simples de peers (MACs) adicionados durante pareamento
#define MAX_PEERS 10
uint8_t knownPeers[MAX_PEERS][6];
int knownPeerCount = 0;

// helper para checar se MAC já está na lista
bool knownPeerContains(const uint8_t *mac) {
	for (int i = 0; i < knownPeerCount; ++i) {
		bool same = true;
		for (int j = 0; j < 6; ++j) if (knownPeers[i][j] != mac[j]) { same = false; break; }
		if (same) return true;
	}
	return false;
}

// adiciona MAC à lista local (não substitui esp_now_add_peer, apenas mantém lista para envio)
void addKnownPeerToList(const uint8_t *mac) {
	if (knownPeerCount >= MAX_PEERS) return;
	if (knownPeerContains(mac)) return;
	memcpy(knownPeers[knownPeerCount++], mac, 6);
}

// Mensagens: 1 = PAIR, 2 = STATE
typedef struct __attribute__((packed)) {
	uint8_t type;   // 1=pair, 2=state
	uint8_t pin;    // número do pino (ex: 2)
	uint8_t value;  // 0 ou 1
	uint32_t seq;   // contador monotônico do remetente
} Packet;

Packet pkt;

uint32_t localSeq = 1; // incrementa a cada alteração local
uint32_t lastSeqForPin[40]; // índices por número de pino (só usamos pino 2 aqui)
bool ignoreNextChangeForPin[40]; // quando aplicamos uma alteração recebida, ignorar o evento local subsequente

bool lastButton = HIGH, btnFlag = false;

// helper: retorna índice simples para pino (usamos pinos pequenos, índice = pino)
inline int pinIndex(uint8_t pin) { return pin; }

// ====== Função para verificar se peer já está adicionado ======
bool peerExiste(const uint8_t *mac) {
	esp_now_peer_info_t peer;
	return esp_now_get_peer(mac, &peer) == ESP_OK;
}

// ====== Função para adicionar peer dinamicamente ======
// adiciona peer com criptografia habilitada (usa LMK definido acima)
bool adicionaPeer(const uint8_t *mac) {
	esp_now_peer_info_t peerInfo = {};
	memcpy(peerInfo.peer_addr, mac, 6);
	peerInfo.channel = 0;
	peerInfo.encrypt = true; // habilita criptografia ESP-NOW
	memcpy(peerInfo.lmk, LMK, 16);

	if (!peerExiste(mac)) {
		Serial.print("Adicionando peer (encrypt): ");
		for(int i=0; i<6; i++){ Serial.printf("%02X:", mac[i]); }
		Serial.println();

		bool ok = esp_now_add_peer(&peerInfo) == ESP_OK;
		if (ok) addKnownPeerToList(mac);
		return ok;
	}
	// já existia: garante que também está na lista local
	addKnownPeerToList(mac);
	return true;
}

void OnSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
	Serial.print("Status do envio: ");
	Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCESSO" : "FALHA");
}

// ====== Callback de recebimento ======
void OnRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
	if (len < (int)sizeof(Packet)) return;
	Packet r;
	memcpy(&r, incomingData, sizeof(Packet));

	Serial.print("Recebido tipo="); Serial.print(r.type);
	Serial.print(" pin="); Serial.print(r.pin);
	Serial.print(" val="); Serial.print(r.value);
	Serial.print(" seq="); Serial.println(r.seq);

	// Salvar MAC se não estiver pareado
	if (!peerExiste(mac)) {
		Serial.println("Novo dispositivo encontrado, salvando (com encrypt)...");
		if (adicionaPeer(mac)) {
			Serial.println("Peer adicionado!");
		} else {
			Serial.println("Falha ao adicionar peer!");
			return;
		}
	} else {
		// já existia -> garante na lista local também
		addKnownPeerToList(mac);
	}

	if (r.type == 1) {
		// Mensagem de pareamento — pode ser ignorada além de adicionar peer
		return;
	}

	if (r.type == 2) {
		int idx = pinIndex(r.pin);
		// aceitar somente se seq for mais recente
		if (r.seq > lastSeqForPin[idx]) {
			lastSeqForPin[idx] = r.seq;
			// aplicar valor no pino
			Serial.print("Aplicando pino "); Serial.print(r.pin); Serial.print(" = "); Serial.println(r.value);
			ignoreNextChangeForPin[idx] = true;
			digitalWrite(r.pin, r.value ? HIGH : LOW);
		} else {
			Serial.println("Mensagem antiga ignorada");
		}
	}
}

// Envia estado atual do pino (broadcast)
// Envia estado do pino para TODOS os peers conhecidos usando envio para MAC (encriptado)
void sendState(uint8_t pin, uint8_t value) {
	if (knownPeerCount == 0) {
		Serial.println("Nenhum peer conhecido — consider use pair broadcast first");
	}

	Packet s;
	s.type = 2;
	s.pin = pin;
	s.value = value;
	s.seq = ++localSeq;

	for (int i = 0; i < knownPeerCount; ++i) {
		uint8_t *mac = knownPeers[i];
		esp_err_t res = esp_now_send(mac, (uint8_t*)&s, sizeof(s));
		if (res != ESP_OK) {
			Serial.print("Erro esp_now_send para peer ");
			for (int j=0;j<6;j++){ Serial.printf("%02X", mac[j]); if (j<5) Serial.print(":"); }
			Serial.print(" : "); Serial.println(res);
		}
	}
}

void setup() {
	Serial.begin(115200);
	pinMode(led_pin, OUTPUT);
	pinMode(bot_pin, INPUT_PULLUP);

	// garantir estado inicial
	digitalWrite(led_pin, LOW);

	WiFi.mode(WIFI_STA);
	WiFi.disconnect();

	if (esp_now_init() != ESP_OK) {
		Serial.println("Erro ao iniciar ESP-NOW");
		return;
	}

	esp_now_register_send_cb(OnSent);
	esp_now_register_recv_cb(OnRecv);

	// Set global PMK (Primary Master Key) para criptografia ESP-NOW
	if (esp_now_set_pmk(PMK) != ESP_OK) {
		Serial.println("Falha ao setar PMK");
	} else {
		Serial.println("PMK definido");
	}

	// Necessário para conseguir broadcast
	esp_now_peer_info_t peerInfo = {};
	memcpy(peerInfo.peer_addr, broadcastAddress, 6);
	peerInfo.channel = 0;
	peerInfo.encrypt = false;
  
	if (esp_now_add_peer(&peerInfo) != ESP_OK) {
		Serial.println("Falha ao adicionar peer broadcast");
	}

	delay(2000);
	// se apertar botão no boot, envie pedido de pareamento
	if (!digitalRead(bot_pin)){
		Packet p;
		p.type = 1; // pair
		esp_now_send(broadcastAddress, (uint8_t*)&p, sizeof(p));
		Serial.println("Enviado: pair broadcast");
		delay(500);
	}
}

void loop() {
	delay(10);

	bool cur = digitalRead(bot_pin);

	if (lastButton == HIGH && cur == LOW) btnFlag = true;         // apertou
	if (lastButton == LOW && cur == HIGH && btnFlag) {            // soltou
		btnFlag = false;

		// Toggle local LED e envie estado
		int idx = pinIndex(led_pin);
		// Se a alteração veio de uma mensagem recebida, ela setou ignoreNextChange
		if (ignoreNextChangeForPin[idx]) {
			// limpar flag e não enviar — foi uma aplicação de estado remoto
			ignoreNextChangeForPin[idx] = false;
			Serial.println("Ignorando evento local (aplicado de remoto)");
		} else {
			// mudança originada localmente -> alterna e envia
			int curVal = digitalRead(led_pin);
			int newVal = curVal == HIGH ? LOW : HIGH;
			digitalWrite(led_pin, newVal);
			sendState(led_pin, newVal == HIGH ? 1 : 0);
			// salvar seq local como último conhecido para este pino
			lastSeqForPin[idx] = localSeq;
			Serial.print("Alterado localmente. Enviado estado seq="); Serial.println(localSeq);
		}
	}

	lastButton = cur;
}