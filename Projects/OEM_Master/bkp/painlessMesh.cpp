#include <painlessMesh.h>

#define MESH_PREFIX     "MinhaRede"
#define MESH_PASSWORD   "12345678"
#define MESH_PORT       5555

#define PIN_LED 2
#define PIN_BTN 0
bool ledState = false;
unsigned long lastDebounce = 0;


painlessMesh mesh;

// Estrutura de mensagem
struct Pacote {
  String nomeOrigem;
  String nomeDestino;
  String mensagem;
};

String meuNome = "NODE_A"; // ALTERE ESSE NOME EM CADA DISPOSITIVO

// Tabela dinâmica: nome → nodeID
std::map<String, uint32_t> tabela;

// ======= JSON ENCODER =======

String meshJson(Pacote p) {
  DynamicJsonDocument doc(256);
  doc["origem"] = p.nomeOrigem;
  doc["destino"] = p.nomeDestino;
  doc["msg"] = p.mensagem;
  String json;
  serializeJson(doc, json);
  return json;
}

// ===== CALLBACKS =====

// Dispara quando um nó conecta
void newConnection(uint32_t nodeId) {
  Serial.printf("[+] Conectado: %u\n", nodeId);

  // Reanuncia o próprio nome
  Pacote p;
  p.nomeOrigem = meuNome;
  p.nomeDestino = "ALL";
  p.mensagem   = "ANUNCIO";

  String json = meshJson(p);
  mesh.sendBroadcast(json);
}

// Dispara quando um nó desconecta
void changedConnections() {
  Serial.println("[!] Topologia mudou");

  // Pode reenviar anúncio se necessário
}

// Recebe mensagens
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("\n📩 MSG DE %u: %s\n", from, msg.c_str());

  // Parse básico (espera JSON)
  DynamicJsonDocument doc(256);
  deserializeJson(doc, msg);

  String origem  = doc["origem"];
  String destino = doc["destino"];
  String conteudo = doc["msg"];

  // Se for anúncio, salva ID
  if (conteudo == "ANUNCIO") {
    tabela[origem] = from;
    Serial.printf("🔗 Registrado: %s → %u\n", origem.c_str(), from);
  }
}

// ======= FUNÇÃO PARA ENVIAR MENSAGENS =======

void enviarMensagem(String destino, String texto) {
  Pacote p;
  p.nomeOrigem = meuNome;
  p.nomeDestino = destino;
  p.mensagem = texto;

  String json = meshJson(p);

  if (destino == "ALL") {
    mesh.sendBroadcast(json);
    Serial.println("📤 Broadcast enviado!");
  } else {
    if (tabela.count(destino)) {
      uint32_t nodeId = tabela[destino];
      mesh.sendSingle(nodeId, json);
      Serial.printf("📤 Enviado para %s (%u)\n", destino.c_str(), nodeId);
    } else {
      Serial.println("❌ Destino não conhecido! (ainda não se anunciou)");
    }
  }
}

// ======= SERIAL COMMANDS =======

void serialCommand() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');

  if (cmd.startsWith("sent ")) {
    String dest = cmd.substring(5, cmd.indexOf(" "));
    String msg  = cmd.substring(cmd.indexOf(" ") + 1);
    enviarMensagem(dest, msg);
  }

  if (cmd.startsWith("LED=")) {
  int novo = cmd.substring(4).toInt();
  ledState = novo;
  digitalWrite(PIN_LED, ledState);
  Serial.printf("💡 LED atualizado por rede → %d\n", ledState);
}

}

void checarBotao() {
  if (digitalRead(PIN_BTN) == LOW && millis() - lastDebounce > 300) {
    lastDebounce = millis();

    // Alterna LED localmente
    ledState = !ledState;
    digitalWrite(PIN_LED, ledState);

    Serial.printf("🔘 Botão pressionado -> LED = %d\n", ledState);

    // Envia mudança para todos
    enviarMensagem("NODE_B", String("LED=") + (ledState ? "1" : "0"));
  }
}


// ======= SETUP =======

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);
  bool ledState = false;


  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);

  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.onReceive(receivedCallback);
  mesh.onNewConnection(newConnection);
  mesh.onChangedConnections(changedConnections);

  // Anúncio inicial
  delay(2000);
  Pacote p;
  p.nomeOrigem = meuNome;
  p.nomeDestino = "ALL";
  p.mensagem = "ANUNCIO";
  mesh.sendBroadcast(meshJson(p));
}

// ======= LOOP =======

void loop() {
  mesh.update();
  serialCommand();
  checarBotao();
}
