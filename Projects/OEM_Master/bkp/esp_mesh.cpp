#include <painlessMesh.h>
#include <ArduinoJson.h>

#define MESH_SSID       "MinhaRedeMesh"
#define MESH_PASSWORD   "12345678"
#define MESH_PORT       5555

// 🔹 Nome lógico do nó
String meuNome = "CENTRAL";   // Troque para PORTA_DE, PORTA_DD, etc.

// 🔹 GPIO para exemplo
#define PINO_RELE 2
#define PINO_BOT 0

bool last = HIGH, flag = false;

painlessMesh mesh;

// Mapeamento nome → NodeID
std::map<String, uint32_t> tabela;

// ============================================================
// 🔹 Envia mensagem JSON
// ============================================================
void enviarMensagem(String destino, String comando){
    JsonDocument msg;
    msg["orig"] = meuNome;
    msg["dest"] = destino;
    msg["cmd"]  = comando;

    String pacote;
    serializeJson(msg, pacote);

    // Broadcast
    if(destino == "ALL"){
        mesh.sendBroadcast(pacote);
        return;
    }

    // Verifica se está registrado
    if(!tabela.count(destino)) {
        Serial.println("❌ Destino não registrado ainda!");
        return;
    }

    uint32_t id = tabela[destino];
    Serial.printf("➡ Enviando para %s (%u)\n", destino.c_str(), id);
    mesh.sendSingle(id, pacote);
}

// ============================================================
// 🔹 Recebe mensagens
// ============================================================
void handleMsg(uint32_t from, String &msg){
    JsonDocument doc;
    deserializeJson(doc, msg);

    String destino = doc["dest"];
    String comando = doc["cmd"];


    // ===== Registro de nó =====
    if(doc["type"] == "id") {
    String nome = doc["name"];
    tabela[nome] = from;
    Serial.printf("📍 Registrado: %s -> %u\n", nome.c_str(), from);
    return;
    }

    Serial.printf("📩 msg de %u → %s | cmd = %s\n", from, destino.c_str(), comando.c_str());

    if(destino != meuNome && destino != "ALL") return;

    // comandos
    if(comando == "abrir") digitalWrite(PINO_RELE, HIGH);
    if(comando == "fechar") digitalWrite(PINO_RELE, LOW);
    if(comando == "toggle") digitalWrite(PINO_RELE, !digitalRead(PINO_RELE));

}

// ============================================================
// 🔹 Anuncia identificação quando conecta
// ============================================================
void anunciarID(uint32_t nodeId){
    JsonDocument doc;
    doc["type"] = "id";
    doc["name"] = meuNome;

    String pacote;
    serializeJson(doc, pacote);
    mesh.sendBroadcast(pacote);
    Serial.println("📡 Anunciando meu ID: " + meuNome);
}

// ============================================================
// 🔹 Setup
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PINO_BOT, INPUT_PULLUP);
  pinMode(PINO_RELE, OUTPUT);
  digitalWrite(PINO_RELE, LOW);

  mesh.init(MESH_SSID, MESH_PASSWORD, MESH_PORT);
  mesh.onReceive(handleMsg);

  // Evento quando um novo nó entra
  mesh.onNewConnection([](uint32_t nodeId){
    Serial.printf("🔗 Novo nó detectado: %u\n", nodeId);
    anunciarID(nodeId);
  });
}

// ============================================================
// 🔹 Loop
// ============================================================
void loop() {
    mesh.update();

    // // Envio manual para teste via serial
    // if(Serial.available()){
    // String comando = Serial.readStringUntil('\n');
    // enviarMensagem("PORTA_DE", comando);  // Troque destino
    // }


    bool cur = digitalRead(PINO_BOT);

    if (last == HIGH && cur == LOW) flag = true;         // apertou
    if (last == LOW && cur == HIGH && flag) {            // soltou
        Serial.println("Clicou!");
        flag = false;

        enviarMensagem("P1", "toggle");  // Troque destino

    }

    last = cur;

}
