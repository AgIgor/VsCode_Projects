#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// ============= CONFIGURAÇÕES =============
#define AP_PASSWORD "12345678"
#define AUTH_USER "admin"
#define AUTH_PASS "admin123"
#define DEFAULT_SSID "BCM_ESP32"
#define DEFAULT_BCM_NAME "BCM_ESP32"

// ENTRADAS (Pull-up ativo em LOW)
#define PIN_IGNICAO 13
#define PIN_PORTA 14
#define PIN_TRAVA 25
#define PIN_DESTRAVA 26

// SAÍDAS
#define PIN_FAROL 16
#define PIN_DRL 17
#define PIN_INTERNA 18
#define PIN_PES 19

// FAIL-SAFE
#define TIMEOUT_IGNICAO_OFF 600000  // 600 segundos (10 minutos) em ms

// ============= VARIÁVEIS GLOBAIS =============
WebServer server(80);
Preferences preferences;

// Estado das entradas
bool ignicaoAtual = false;
bool ignicaoAnterior = false;
bool portaAtual = false;
bool portaAnterior = false;
bool travaAtual = false;
bool travaAnterior = false;
bool destrabaAtual = false;
bool destrabaAnterior = false;

// Pulsos (detecção de borda)
bool pulseTrava = false;
bool pulseDestrava = false;

// Timers
unsigned long lastIgnicaoOnTime = 0;
unsigned long timerFarol = 0;
unsigned long timerDRL = 0;
unsigned long timerInterna = 0;
unsigned long timerPes = 0;

// Estado das saídas
bool farolState = false;
bool drlState = false;
bool internaState = false;
bool pesState = false;

// ============= ESTRUTURA DE REGRAS =============
struct Regra {
  String operador;      // "se", "enquanto", "quando"
  String entrada;       // "trava", "destrava", "porta", "ignicao"
  String estado;        // "pulso", "aberta", "fechada", "ligado", "desligado"
  String acao;          // "ligar", "desligar", "ligar_em", "desligar_em"
  int delay_acao;       // delay em segundos
  String saida;         // "farol", "drl", "interna", "pes"
  int duracao;          // por quanto tempo em segundos
  String operadorLogico; // "AND", "OR", ""
  bool ativo;           // regra ativa ou não
};

#define MAX_REGRAS 20
Regra regras[MAX_REGRAS];
int numRegras = 0;

// ============= FUNÇÕES DE LEITURA =============
void lerEntradas() {
  // Salvar estados anteriores
  ignicaoAnterior = ignicaoAtual;
  portaAnterior = portaAtual;
  travaAnterior = travaAtual;
  destrabaAnterior = destrabaAtual;
  
  // Ler estados atuais (Pull-up, ativo em LOW)
  ignicaoAtual = !digitalRead(PIN_IGNICAO);
  portaAtual = !digitalRead(PIN_PORTA);
  travaAtual = !digitalRead(PIN_TRAVA);
  destrabaAtual = !digitalRead(PIN_DESTRAVA);
  
  // Detectar pulsos (borda de subida)
  pulseTrava = (!travaAnterior && travaAtual);
  pulseDestrava = (!destrabaAnterior && destrabaAtual);
  
  // Atualizar timer de ignição
  if (ignicaoAtual) {
    lastIgnicaoOnTime = millis();
  }
}

// ============= FUNÇÕES DE CONTROLE DE SAÍDAS =============
void setSaida(String saida, bool estado) {
  if (saida == "farol") {
    digitalWrite(PIN_FAROL, estado);
    farolState = estado;
  } else if (saida == "drl") {
    digitalWrite(PIN_DRL, estado);
    drlState = estado;
  } else if (saida == "interna") {
    digitalWrite(PIN_INTERNA, estado);
    internaState = estado;
  } else if (saida == "pes") {
    digitalWrite(PIN_PES, estado);
    pesState = estado;
  }
}

// ============= PROCESSAMENTO DE REGRAS =============
void processarRegras() {
  for (int i = 0; i < numRegras; i++) {
    if (!regras[i].ativo) continue;
    
    bool condicao = false;
    
    // Avaliar condição
    if (regras[i].entrada == "trava" && regras[i].estado == "pulso") {
      condicao = pulseTrava;
    } else if (regras[i].entrada == "destrava" && regras[i].estado == "pulso") {
      condicao = pulseDestrava;
    } else if (regras[i].entrada == "porta") {
      if (regras[i].estado == "aberta") condicao = portaAtual;
      else if (regras[i].estado == "fechada") condicao = !portaAtual;
    } else if (regras[i].entrada == "ignicao") {
      if (regras[i].estado == "ligado") condicao = ignicaoAtual;
      else if (regras[i].estado == "desligado") condicao = !ignicaoAtual;
    }
    
    // Executar ação se condição verdadeira
    if (condicao) {
      if (regras[i].operador == "enquanto") {
        // Enquanto: manter saída no estado enquanto condição for verdadeira
        if (regras[i].acao == "ligar") {
          setSaida(regras[i].saida, true);
        } else if (regras[i].acao == "desligar") {
          setSaida(regras[i].saida, false);
        }
      } else if (regras[i].operador == "se" || regras[i].operador == "quando") {
        // Se/Quando: executar ação uma vez
        if (regras[i].acao == "ligar" || regras[i].acao == "ligar_em") {
          // TODO: implementar delay e duração com timers
          setSaida(regras[i].saida, true);
        } else if (regras[i].acao == "desligar" || regras[i].acao == "desligar_em") {
          setSaida(regras[i].saida, false);
        }
      }
    } else {
      // Se condição falsa e é "enquanto", desligar
      if (regras[i].operador == "enquanto" && regras[i].acao == "ligar") {
        setSaida(regras[i].saida, false);
      }
    }
  }
}

// ============= FAIL-SAFE =============
void verificarFailSafe() {
  // Desligar todas as saídas se ignição OFF por muito tempo
  if (!ignicaoAtual && (millis() - lastIgnicaoOnTime) > TIMEOUT_IGNICAO_OFF) {
    setSaida("farol", false);
    setSaida("drl", false);
    setSaida("interna", false);
    setSaida("pes", false);
  }
}

// ============= WEB SERVER =============
const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>BCM ESP32</title>
<style>
:root{--bg:#f6f7fb;--card:#fff;--text:#1c1f2a;--muted:#6b7280;--accent:#2563eb;--border:#e5e7eb;--chip:#eef2ff}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,sans-serif;background:var(--bg);color:var(--text)}
header{padding:20px 16px;background:linear-gradient(120deg,#1d4ed8,#3b82f6);color:#fff}
header h1{margin:0 0 4px;font-size:20px}
header p{margin:0;opacity:.9;font-size:13px}
.container{max-width:1200px;margin:0 auto;padding:16px 16px 60px}
.grid{display:grid;grid-template-columns:1fr;gap:16px}
.card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:16px;box-shadow:0 4px 12px rgba(31,41,55,.05)}
.card h2{margin:0 0 12px;font-size:15px}
.form-row{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px}
.form-row.full{grid-template-columns:1fr}
label{font-size:11px;color:var(--muted);display:block;margin-bottom:5px}
select,input,button{width:100%;border-radius:8px;border:1px solid var(--border);padding:9px 10px;font-size:13px;background:#fff;color:var(--text)}
.rules-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px}
.rule{border:1px dashed var(--border);border-radius:10px;padding:12px;margin-bottom:10px;background:#fafafa}
.rule-title{display:flex;justify-content:space-between;align-items:center;font-size:12px;color:var(--muted);margin-bottom:8px}
.rule-grid{display:grid;grid-template-columns:1fr;gap:8px}
.chip{display:inline-flex;padding:5px 9px;border-radius:999px;background:var(--chip);color:#1e3a8a;font-size:11px}
.io-list{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.io-item{display:flex;justify-content:space-between;align-items:center;padding:7px 9px;border-radius:8px;background:#f9fafb;border:1px solid var(--border);font-size:12px}
.btn{background:var(--accent);color:#fff;border:none;cursor:pointer;font-weight:600;padding:10px}
.btn.secondary{background:#e5e7eb;color:#111827;font-weight:500}
.btn.danger{background:#dc2626;color:#fff}
.btn-row{display:flex;gap:8px;flex-wrap:wrap}
.btn-row button{flex:1;min-width:120px}
.status{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:5px}
.status.on{background:#10b981}
.status.off{background:#6b7280}
.hidden{display:none}
@media(min-width:980px){.grid{grid-template-columns:1.1fr 1.6fr}.rule-grid{grid-template-columns:repeat(6,1fr)}}
</style>
</head>
<body>
<header>
<h1>BCM ESP32 - Modulo Automatizador</h1>
<p>Configure regras e monitore entradas/saídas</p>
</header>
<main class="container">
<div class="grid">
<section class="card">
<h2>Status Entradas</h2>
<div class="io-list">
<div class="io-item"><span><span class="status off" id="st-trava"></span>Trava</span><strong>GPIO 25</strong></div>
<div class="io-item"><span><span class="status off" id="st-destrava"></span>Destrava</span><strong>GPIO 26</strong></div>
<div class="io-item"><span><span class="status off" id="st-porta"></span>Porta</span><strong>GPIO 14</strong></div>
<div class="io-item"><span><span class="status off" id="st-ignicao"></span>Ignição</span><strong>GPIO 13</strong></div>
</div>
<h2 style="margin-top:14px">Status Saídas</h2>
<div class="io-list">
<div class="io-item"><span><span class="status off" id="st-farol"></span>Farol</span><strong>GPIO 16</strong></div>
<div class="io-item"><span><span class="status off" id="st-drl"></span>DRL</span><strong>GPIO 17</strong></div>
<div class="io-item"><span><span class="status off" id="st-interna"></span>Interna</span><strong>GPIO 18</strong></div>
<div class="io-item"><span><span class="status off" id="st-pes"></span>Pés</span><strong>GPIO 19</strong></div>
</div>
</section>
<section class="card">
<div class="rules-header">
<h2>Regras (<span id="rule-count">0</span>)</h2>
<button class="btn" onclick="addRule()">+ Nova Regra</button>
</div>
<div id="rules-container"></div>
<div class="btn-row" style="margin-top:12px">
<button class="btn" onclick="saveConfig()">Salvar</button>
<button class="btn secondary" onclick="loadConfig()">Recarregar</button>
<button class="btn secondary" onclick="exportConfig()">Exportar</button>
<button class="btn danger" onclick="clearAll()">Limpar Tudo</button>
</div>
</section>
</div>
</main>
<script>
let rules=[];
let ruleIdCounter=0;
function addRule(){
const id=ruleIdCounter++;
const rule={id,operador:'se',entrada:'porta',estado:'aberta',acao:'ligar',delay:0,saida:'interna',duracao:0,logico:'',ativo:true};
rules.push(rule);
renderRules();
}
function removeRule(id){
rules=rules.filter(r=>r.id!==id);
renderRules();
}
function updateRule(id,field,value){
const rule=rules.find(r=>r.id===id);
if(rule){
rule[field]=value;
if(field==='entrada'){
if(value==='trava'||value==='destrava')rule.estado='pulso';
else if(value==='porta')rule.estado='aberta';
else if(value==='ignicao')rule.estado='ligado';
}
renderRules();
}
}
function renderRules(){
const container=document.getElementById('rules-container');
container.innerHTML='';
rules.forEach((r,idx)=>{
const div=document.createElement('div');
div.className='rule';
const estadoOpts=getEstadoOptions(r.entrada);
div.innerHTML=`
<div class="rule-title">
<span>Regra ${idx+1}</span>
<button class="btn danger" style="padding:4px 10px;font-size:11px" onclick="removeRule(${r.id})">Remover</button>
</div>
<div class="rule-grid">
<div><label>Operador</label>
<select onchange="updateRule(${r.id},'operador',this.value)">
<option ${r.operador==='se'?'selected':''}>se</option>
<option ${r.operador==='enquanto'?'selected':''}>enquanto</option>
<option ${r.operador==='quando'?'selected':''}>quando</option>
</select></div>
<div><label>Entrada</label>
<select onchange="updateRule(${r.id},'entrada',this.value)">
<option ${r.entrada==='porta'?'selected':''}>porta</option>
<option ${r.entrada==='trava'?'selected':''}>trava</option>
<option ${r.entrada==='destrava'?'selected':''}>destrava</option>
<option ${r.entrada==='ignicao'?'selected':''}>ignicao</option>
</select></div>
<div><label>Estado</label>
<select onchange="updateRule(${r.id},'estado',this.value)">${estadoOpts}</select></div>
<div><label>Ação</label>
<select onchange="updateRule(${r.id},'acao',this.value)">
<option ${r.acao==='ligar'?'selected':''}>ligar</option>
<option ${r.acao==='desligar'?'selected':''}>desligar</option>
<option ${r.acao==='ligar_em'?'selected':''}>ligar_em</option>
<option ${r.acao==='desligar_em'?'selected':''}>desligar_em</option>
</select></div>
<div><label>Delay (s)</label>
<input type="number" value="${r.delay}" onchange="updateRule(${r.id},'delay',parseInt(this.value)||0)"/></div>
<div><label>Saída</label>
<select onchange="updateRule(${r.id},'saida',this.value)">
<option ${r.saida==='farol'?'selected':''}>farol</option>
<option ${r.saida==='drl'?'selected':''}>drl</option>
<option ${r.saida==='interna'?'selected':''}>interna</option>
<option ${r.saida==='pes'?'selected':''}>pes</option>
</select></div>
</div>
<div class="form-row" style="margin-top:8px">
<div><label>Duração (s)</label>
<input type="number" value="${r.duracao}" onchange="updateRule(${r.id},'duracao',parseInt(this.value)||0)"/></div>
<div><label>Op. Lógico</label>
<select onchange="updateRule(${r.id},'logico',this.value)">
<option ${r.logico===''?'selected':''}>—</option>
<option ${r.logico==='AND'?'selected':''}>AND</option>
<option ${r.logico==='OR'?'selected':''}>OR</option>
</select></div>
</div>`;
container.appendChild(div);
});
document.getElementById('rule-count').textContent=rules.length;
}
function getEstadoOptions(entrada){
if(entrada==='trava'||entrada==='destrava')return'<option>pulso</option>';
if(entrada==='porta')return'<option>aberta</option><option>fechada</option>';
if(entrada==='ignicao')return'<option>ligado</option><option>desligado</option>';
return'';
}
function saveConfig(){
fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({rules})})
.then(r=>r.json()).then(d=>{alert(d.message||'Salvo com sucesso!')}).catch(e=>alert('Erro ao salvar'));
}
function loadConfig(){
fetch('/api/config').then(r=>r.json()).then(d=>{rules=d.rules||[];ruleIdCounter=rules.length?Math.max(...rules.map(r=>r.id))+1:0;renderRules()}).catch(e=>console.error(e));
}
function exportConfig(){
const blob=new Blob([JSON.stringify({rules},null,2)],{type:'application/json'});
const url=URL.createObjectURL(blob);
const a=document.createElement('a');
a.href=url;
a.download='bcm_config.json';
a.click();
}
function clearAll(){
if(confirm('Limpar todas as regras?')){rules=[];renderRules();}
}
function updateStatus(){
fetch('/api/status').then(r=>r.json()).then(d=>{
['trava','destrava','porta','ignicao','farol','drl','interna','pes'].forEach(k=>{
const el=document.getElementById('st-'+k);
if(el)el.className='status '+(d[k]?'on':'off');
});
}).catch(e=>console.error(e));
}
loadConfig();
setInterval(updateStatus,1000);
updateStatus();
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleGetStatus() {
  StaticJsonDocument<512> doc;
  doc["ignicao"] = ignicaoAtual;
  doc["porta"] = portaAtual;
  doc["trava"] = travaAtual;
  doc["destrava"] = destrabaAtual;
  doc["farol"] = farolState;
  doc["drl"] = drlState;
  doc["interna"] = internaState;
  doc["pes"] = pesState;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleGetConfig() {
  DynamicJsonDocument doc(4096);
  JsonArray rulesArray = doc.createNestedArray("rules");
  
  for (int i = 0; i < numRegras; i++) {
    JsonObject r = rulesArray.createNestedObject();
    r["id"] = i;
    r["operador"] = regras[i].operador;
    r["entrada"] = regras[i].entrada;
    r["estado"] = regras[i].estado;
    r["acao"] = regras[i].acao;
    r["delay"] = regras[i].delay_acao;
    r["saida"] = regras[i].saida;
    r["duracao"] = regras[i].duracao;
    r["logico"] = regras[i].operadorLogico;
    r["ativo"] = regras[i].ativo;
  }
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handlePostConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Body vazio\"}");
    return;
  }
  
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"JSON inválido\"}");
    return;
  }
  
  JsonArray rulesArray = doc["rules"];
  numRegras = 0;
  
  for (JsonObject r : rulesArray) {
    if (numRegras >= MAX_REGRAS) break;
    
    regras[numRegras].operador = r["operador"].as<String>();
    regras[numRegras].entrada = r["entrada"].as<String>();
    regras[numRegras].estado = r["estado"].as<String>();
    regras[numRegras].acao = r["acao"].as<String>();
    regras[numRegras].delay_acao = r["delay"] | 0;
    regras[numRegras].saida = r["saida"].as<String>();
    regras[numRegras].duracao = r["duracao"] | 0;
    regras[numRegras].operadorLogico = r["logico"].as<String>();
    regras[numRegras].ativo = r["ativo"] | true;
    numRegras++;
  }
  
  // Salvar na memória flash
  preferences.begin("bcm", false);
  preferences.putInt("numRegras", numRegras);
  
  for (int i = 0; i < numRegras; i++) {
    String prefix = "r" + String(i) + "_";
    preferences.putString((prefix + "op").c_str(), regras[i].operador);
    preferences.putString((prefix + "ent").c_str(), regras[i].entrada);
    preferences.putString((prefix + "est").c_str(), regras[i].estado);
    preferences.putString((prefix + "aca").c_str(), regras[i].acao);
    preferences.putInt((prefix + "del").c_str(), regras[i].delay_acao);
    preferences.putString((prefix + "sai").c_str(), regras[i].saida);
    preferences.putInt((prefix + "dur").c_str(), regras[i].duracao);
    preferences.putString((prefix + "log").c_str(), regras[i].operadorLogico);
    preferences.putBool((prefix + "ati").c_str(), regras[i].ativo);
  }
  
  preferences.end();
  
  Serial.println("Configuração salva: " + String(numRegras) + " regras");
  server.send(200, "application/json", "{\"message\":\"Configuração salva com sucesso\"}");
}

// ============= SETUP =============
void setup() {
  Serial.begin(115200);
  Serial.println("\n\nBCM ESP32 - Iniciando...");
  
  // Configurar pinos de entrada (Pull-up interno)
  pinMode(PIN_IGNICAO, INPUT_PULLUP);
  pinMode(PIN_PORTA, INPUT_PULLUP);
  pinMode(PIN_TRAVA, INPUT_PULLUP);
  pinMode(PIN_DESTRAVA, INPUT_PULLUP);
  
  // Configurar pinos de saída
  pinMode(PIN_FAROL, OUTPUT);
  pinMode(PIN_DRL, OUTPUT);
  pinMode(PIN_INTERNA, OUTPUT);
  pinMode(PIN_PES, OUTPUT);
  
  // Desligar todas as saídas
  digitalWrite(PIN_FAROL, LOW);
  digitalWrite(PIN_DRL, LOW);
  digitalWrite(PIN_INTERNA, LOW);
  digitalWrite(PIN_PES, LOW);
  
  // Iniciar WiFi em modo AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DEFAULT_SSID, AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  // Configurar rotas do servidor web
  server.on("/", handleRoot);
  server.on("/api/status", handleGetStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  
  server.begin();
  Serial.println("Web server iniciado");
  
  // Carregar configurações salvas
  preferences.begin("bcm", false);
  numRegras = preferences.getInt("numRegras", 0);
  
  for (int i = 0; i < numRegras; i++) {
    String prefix = "r" + String(i) + "_";
    regras[i].operador = preferences.getString((prefix + "op").c_str(), "se");
    regras[i].entrada = preferences.getString((prefix + "ent").c_str(), "porta");
    regras[i].estado = preferences.getString((prefix + "est").c_str(), "aberta");
    regras[i].acao = preferences.getString((prefix + "aca").c_str(), "ligar");
    regras[i].delay_acao = preferences.getInt((prefix + "del").c_str(), 0);
    regras[i].saida = preferences.getString((prefix + "sai").c_str(), "interna");
    regras[i].duracao = preferences.getInt((prefix + "dur").c_str(), 0);
    regras[i].operadorLogico = preferences.getString((prefix + "log").c_str(), "");
    regras[i].ativo = preferences.getBool((prefix + "ati").c_str(), true);
  }
  
  preferences.end();
  
  Serial.println("Regras carregadas: " + String(numRegras));
  Serial.println("Sistema pronto!");
}

// ============= LOOP =============
void loop() {
  server.handleClient();
  
  lerEntradas();
  processarRegras();
  verificarFailSafe();
  
  delay(50);  // Debounce e não sobrecarregar o processador
}