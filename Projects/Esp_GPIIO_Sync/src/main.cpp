#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#define bot_pin 0
#define led_pin 2

// broadcast para descoberta inicial
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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
		Serial.println("Novo dispositivo encontrado, salvando...");
		if (adicionaPeer(mac)) {
			Serial.println("Peer adicionado!");
		} else {
			Serial.println("Falha ao adicionar peer!");
			return;
		}
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
void sendState(uint8_t pin, uint8_t value) {
	Packet s;
	s.type = 2;
	s.pin = pin;
	s.value = value;
	s.seq = ++localSeq;

	esp_err_t res = esp_now_send(broadcastAddress, (uint8_t*)&s, sizeof(s));
	if (res != ESP_OK) {
		Serial.print("Erro esp_now_send: "); Serial.println(res);
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