#include <WebServer.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "ServerManager.h"
#include "IOManager.h"
#include "LadderEngine.h"

#ifndef WIFI_STA_SSID
#define WIFI_STA_SSID ""
#endif

#ifndef WIFI_STA_PASS
#define WIFI_STA_PASS ""
#endif

static constexpr const char* AP_SSID = "ESP32-CLP";
static constexpr const char* AP_PASS = "12345678";

ServerManager serverManager;

// SPA minimalista para edição de blocos e monitoramento.
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>ESP32 CLP</title>
    <style>
        :root {
            --bg: #0f172a;
            --panel: #111827;
            --card: #1f2937;
            --accent: #38bdf8;
            --accent-2: #22c55e;
            --text: #e5e7eb;
            --muted: #94a3b8;
            --border: #1f2937;
            font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
        }
        * { box-sizing: border-box; }
        body { background: radial-gradient(circle at 20% 20%, #0b1831, #070d1d 60%); color: var(--text); margin: 0; padding: 16px; }
        h1 { margin: 0; font-size: 24px; letter-spacing: 0.5px; }
        .layout { max-width: 1100px; margin: 0 auto; display: grid; gap: 16px; }
        header { background: var(--panel); border: 1px solid var(--border); padding: 14px 16px; border-radius: 12px; display: flex; flex-wrap: wrap; gap: 12px; align-items: center; justify-content: space-between; }
        .status { display: flex; align-items: center; gap: 10px; color: var(--muted); }
        .pill { padding: 6px 10px; background: rgba(56,189,248,0.1); border: 1px solid rgba(56,189,248,0.4); border-radius: 999px; color: var(--text); }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 12px; }
        .card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 14px; }
        .card h2 { margin: 0 0 10px 0; font-size: 14px; letter-spacing: 0.2px; color: var(--muted); text-transform: uppercase; }
        .io-row { display: flex; justify-content: space-between; align-items: center; padding: 8px 10px; border-radius: 8px; border: 1px solid var(--border); margin-bottom: 6px; background: #0b1224; }
        .badge { padding: 4px 8px; border-radius: 8px; font-weight: 600; font-size: 12px; min-width: 52px; text-align: center; }
        .on { background: rgba(34,197,94,0.2); color: #86efac; border: 1px solid rgba(34,197,94,0.4); }
        .off { background: rgba(239,68,68,0.2); color: #fca5a5; border: 1px solid rgba(239,68,68,0.4); }
        button { cursor: pointer; border: 1px solid var(--border); border-radius: 10px; padding: 10px 14px; background: #0ea5e9; color: #0b1224; font-weight: 700; transition: transform 0.08s ease, background 0.2s ease; }
        button.secondary { background: #1f2937; color: var(--text); }
        button.danger { background: #ef4444; color: #0b1224; }
        button:hover { transform: translateY(-1px); }
        button:active { transform: translateY(0); }
        .editor { background: var(--panel); border: 1px solid var(--border); border-radius: 12px; padding: 12px; }
        .palette { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 8px; margin-bottom: 10px; }
        .block-chip { border: 1px dashed rgba(56,189,248,0.5); padding: 8px; border-radius: 10px; text-align: center; background: rgba(56,189,248,0.08); color: var(--text); font-weight: 600; user-select: none; }
        .canvas { border: 2px dashed rgba(148,163,184,0.3); padding: 10px; min-height: 140px; border-radius: 12px; background: #0b1224; margin-bottom: 10px; }
        .block { border: 1px solid var(--border); background: #111827; border-radius: 10px; padding: 8px; margin-bottom: 8px; }
        .block header { display: flex; justify-content: space-between; align-items: center; background: transparent; border: none; padding: 0; }
        .block-title { font-weight: 700; color: var(--text); }
        .block form { display: grid; grid-template-columns: repeat(auto-fit, minmax(110px, 1fr)); gap: 6px; margin-top: 6px; }
        label { font-size: 11px; color: var(--muted); display: flex; flex-direction: column; gap: 4px; }
        input, select { border-radius: 8px; border: 1px solid var(--border); background: #0b1224; color: var(--text); padding: 6px; }
        textarea { width: 100%; min-height: 120px; border-radius: 10px; border: 1px solid var(--border); background: #0b1224; color: var(--text); padding: 10px; font-family: 'JetBrains Mono', monospace; font-size: 12px; }
        .actions { display: flex; flex-wrap: wrap; gap: 8px; }
    </style>
</head>
<body>
    <div class="layout">
        <header>
            <h1>ESP32 CLP • Ladder / Blocos</h1>
            <div class="status">
                <span class="pill" id="netInfo">NET: --</span>
                <span class="pill" id="cycleLabel">Ciclo: -- ms</span>
                <label>Ciclo (10-100 ms)
                    <input id="cycleMs" type="number" min="10" max="100" value="20" />
                </label>
                <button class="secondary" onclick="updateCycle()">Aplicar</button>
            </div>
        </header>

        <div class="grid">
            <div class="card">
                <h2>Entradas (pull-up, ativo em LOW)</h2>
                <div id="inputs"></div>
            </div>
            <div class="card">
                <h2>Saídas (nível HIGH)</h2>
                <div id="outputs"></div>
            </div>
        </div>

        <div class="editor">
            <h2 style="margin: 0 0 8px 0;">Editor gráfico (arraste blocos)</h2>
            <div class="palette" id="palette"></div>
            <div id="canvas" class="canvas">Arraste blocos aqui ou clique em um bloco na paleta.</div>
            <textarea id="programJson" spellcheck="false"></textarea>
            <div class="actions">
                <button onclick="loadProgram()">Carregar</button>
                <button onclick="saveProgram()">Salvar</button>
                <button class="secondary" onclick="pushToRuntime()">Aplicar na RAM</button>
                <button class="danger" onclick="eraseProgram()">Apagar</button>
            </div>
        </div>
    </div>

    <script>
        const palette = [
            { type: 'CONTACT_NO', label: 'Contato NA' },
            { type: 'CONTACT_NC', label: 'Contato NF' },
            { type: 'COIL', label: 'Bobina' },
            { type: 'AND', label: 'AND' },
            { type: 'OR', label: 'OR' },
            { type: 'NOT', label: 'NOT' },
            { type: 'TIMER_ON', label: 'Timer ON' },
            { type: 'TIMER_OFF', label: 'Timer OFF' },
            { type: 'LATCH_SET', label: 'Latch SET' },
            { type: 'LATCH_RESET', label: 'Latch RESET' },
            { type: 'CONST_TRUE', label: 'Const TRUE' },
            { type: 'CONST_FALSE', label: 'Const FALSE' }
        ];

        let blocks = [];
        let nextId = 0;

        const $ = (id) => document.getElementById(id);

        function renderPalette() {
            const p = $('palette');
            palette.forEach(item => {
                const chip = document.createElement('div');
                chip.className = 'block-chip';
                chip.draggable = true;
                chip.textContent = item.label;
                chip.dataset.type = item.type;
                chip.addEventListener('dragstart', (e) => {
                    e.dataTransfer.setData('text/plain', item.type);
                });
                chip.addEventListener('click', () => addBlock(item.type));
                p.appendChild(chip);
            });
        }

        function addBlock(type) {
            blocks.push({
                id: nextId++,
                type,
                a: -1,
                b: -1,
                io: 0,
                delay_ms: type.includes('TIMER') ? 500 : 0
            });
            renderBlocks();
        }

        function renderBlocks() {
            const canvas = $('canvas');
            canvas.innerHTML = '';
            blocks.forEach((b, idx) => {
                const card = document.createElement('div');
                card.className = 'block';

                const head = document.createElement('header');
                const title = document.createElement('div');
                title.className = 'block-title';
                title.textContent = `${b.type} (#${idx})`;
                const removeBtn = document.createElement('button');
                removeBtn.className = 'secondary';
                removeBtn.textContent = 'Excluir';
                removeBtn.onclick = () => { blocks.splice(idx, 1); renderBlocks(); };
                head.appendChild(title);
                head.appendChild(removeBtn);
                card.appendChild(head);

                const form = document.createElement('form');
                form.onsubmit = (e) => e.preventDefault();

                form.appendChild(makeSelect('Tipo', b.type, palette.map(p => p.type), (v) => { b.type = v; renderBlocks(); }));
                form.appendChild(makeNumber('Fonte A (id)', b.a, (v) => b.a = v));
                form.appendChild(makeNumber('Fonte B (id)', b.b, (v) => b.b = v));
                form.appendChild(makeNumber('IO (0..5)', b.io, (v) => b.io = v));
                form.appendChild(makeNumber('Delay ms', b.delay_ms, (v) => b.delay_ms = v));

                card.appendChild(form);
                canvas.appendChild(card);
            });
            updateProgramJson();
        }

        function makeSelect(labelTxt, value, options, onChange) {
            const label = document.createElement('label');
            label.textContent = labelTxt;
            const select = document.createElement('select');
            options.forEach(op => {
                const o = document.createElement('option');
                o.value = op; o.textContent = op; if (op === value) o.selected = true; select.appendChild(o);
            });
            select.onchange = (e) => onChange(e.target.value);
            label.appendChild(select);
            return label;
        }

        function makeNumber(labelTxt, value, onChange) {
            const label = document.createElement('label');
            label.textContent = labelTxt;
            const input = document.createElement('input');
            input.type = 'number';
            input.value = value;
            input.onchange = (e) => onChange(parseInt(e.target.value));
            label.appendChild(input);
            return label;
        }

        function updateProgramJson() {
            const cycle = parseInt($('cycleMs').value || '20');
            const obj = { cycle_ms: cycle, blocks };
            $('programJson').value = JSON.stringify(obj, null, 2);
        }

        function setProgramFromJson(text) {
            try {
                const data = JSON.parse(text);
                blocks = (data.blocks || []).map(b => ({
                    id: b.id ?? 0,
                    type: b.type,
                    a: b.a ?? -1,
                    b: b.b ?? -1,
                    io: b.io ?? 0,
                    delay_ms: b.delay_ms ?? 0
                }));
                nextId = blocks.length;
                $('cycleMs').value = data.cycle_ms ?? 20;
                renderBlocks();
            } catch (e) {
                alert('JSON inválido: ' + e);
            }
        }

        function loadProgram() {
            fetch('/program').then(r => r.json()).then(data => {
                setProgramFromJson(JSON.stringify(data));
            }).catch(err => alert('Erro ao carregar: ' + err));
        }

        function saveProgram() {
            const json = $('programJson').value;
            fetch('/program', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: json })
                .then(r => r.text())
                .then(msg => alert(msg))
                .catch(err => alert('Erro ao salvar: ' + err));
        }

        function pushToRuntime() {
            const json = $('programJson').value;
            fetch('/program', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: json })
                .then(r => r.text())
                .then(msg => alert('Aplicado: ' + msg))
                .catch(err => alert('Erro: ' + err));
        }

        function eraseProgram() {
            if (!confirm('Apagar programa salvo?')) return;
            fetch('/program', { method: 'DELETE' })
                .then(r => r.text())
                .then(msg => alert(msg))
                .catch(err => alert('Erro: ' + err));
        }

        function updateCycle() {
            const ms = parseInt($('cycleMs').value || '20');
            fetch('/cycle', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ ms }) })
                .then(r => r.text())
                .then(() => $('cycleLabel').textContent = `Ciclo: ${ms} ms`)
                .catch(err => alert('Erro ciclo: ' + err));
        }

        function toggleOutput(idx, state) {
            fetch('/setOutput', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ index: idx, state }) });
        }

        function refreshStatus() {
            fetch('/status').then(r => r.json()).then(data => {
                $('cycleLabel').textContent = `Ciclo: ${data.cycle_ms} ms`;
                $('cycleMs').value = data.cycle_ms;
                $('netInfo').textContent = `NET: ${data.mode} ${data.ssid} ${data.ip}`;
                const inDiv = $('inputs'); inDiv.innerHTML = '';
                data.inputs.forEach((v, i) => {
                    const row = document.createElement('div');
                    row.className = 'io-row';
                    row.innerHTML = `<span>IN${i}</span><span class="badge ${v ? 'on' : 'off'}">${v ? 'ON' : 'OFF'}</span>`;
                    inDiv.appendChild(row);
                });
                const outDiv = $('outputs'); outDiv.innerHTML = '';
                data.outputs.forEach((v, i) => {
                    const row = document.createElement('div');
                    row.className = 'io-row';
                    const btn = document.createElement('button');
                    btn.className = 'secondary';
                    btn.textContent = v ? 'ON' : 'OFF';
                    btn.onclick = () => toggleOutput(i, !v);
                    row.innerHTML = `<span>OUT${i}</span>`;
                    row.appendChild(btn);
                    outDiv.appendChild(row);
                });
            }).catch(err => console.error(err));
        }

        function setupDnD() {
            const canvas = $('canvas');
            canvas.addEventListener('dragover', (e) => e.preventDefault());
            canvas.addEventListener('drop', (e) => {
                e.preventDefault();
                const type = e.dataTransfer.getData('text/plain');
                addBlock(type);
            });
        }

        function init() {
            renderPalette();
            renderBlocks();
            setupDnD();
            refreshStatus();
            setInterval(refreshStatus, 800);
        }

        window.addEventListener('load', init);
    </script>
</body>
</html>
)rawliteral";

ServerManager::ServerManager() : server(80), apMode(true) {}

void ServerManager::init(const char* staSsid, const char* staPass, bool forceAp) {
    auto startAp = [&]() {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASS);
        apMode = true;
        currentSsid = AP_SSID;
        currentIp = WiFi.softAPIP();
        Serial.printf("[WiFi] AP iniciado em %s\n", currentIp.toString().c_str());
    };

    const char* ssid = (staSsid && strlen(staSsid) > 0) ? staSsid : WIFI_STA_SSID;
    const char* pass = (staPass && strlen(staPass) > 0) ? staPass : WIFI_STA_PASS;

    if (!forceAp && strlen(ssid) > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, pass);
        Serial.printf("[WiFi] Conectando STA em %s...\n", ssid);
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
            delay(100);
        }
        if (WiFi.status() == WL_CONNECTED) {
            apMode = false;
            currentSsid = ssid;
            currentIp = WiFi.localIP();
            Serial.printf("[WiFi] STA conectado em %s IP %s\n", ssid, currentIp.toString().c_str());
        } else {
            Serial.println("[WiFi] STA falhou, caindo para AP");
            startAp();
        }
    } else {
        startAp();
    }

    setupRoutes();
}

void ServerManager::setupRoutes() {
        server.on("/", HTTP_GET, [this]() {
                server.sendHeader("Access-Control-Allow-Origin", "*");
                server.send_P(200, "text/html", INDEX_HTML);
        });

        server.on("/status", HTTP_GET, [this]() {
                server.sendHeader("Access-Control-Allow-Origin", "*");
                StaticJsonDocument<256> doc;
                bool ins[INPUTS_COUNT];
                bool outs[OUTPUTS_COUNT];
                ioManager.getAllInputs(ins);
                ioManager.getAllOutputs(outs);

                JsonArray aIn = doc.createNestedArray("inputs");
                for (uint8_t i = 0; i < INPUTS_COUNT; i++) aIn.add(ins[i]);
                JsonArray aOut = doc.createNestedArray("outputs");
                for (uint8_t i = 0; i < OUTPUTS_COUNT; i++) aOut.add(outs[i]);
                doc["cycle_ms"] = ladderEngine.getCycleMs();
                doc["mode"] = apMode ? "AP" : "STA";
                doc["ssid"] = currentSsid;
                doc["ip"] = currentIp.toString();

                String res;
                serializeJson(doc, res);
                server.sendHeader("Content-Type", "application/json");
                server.send(200, "application/json", res);
        });

        server.on("/setOutput", HTTP_POST, [this]() {
                server.sendHeader("Access-Control-Allow-Origin", "*");
                if (!server.hasArg("plain")) { server.send(400, "text/plain", "Bad Request"); return; }
                StaticJsonDocument<128> doc;
                auto err = deserializeJson(doc, server.arg("plain"));
                if (err) { server.send(400, "text/plain", "JSON error"); return; }
                int index = doc["index"] | -1;
                bool state = doc.containsKey("state") ? doc["state"].as<bool>() : !ioManager.getOutput(index);
                if (index >= 0 && index < OUTPUTS_COUNT) {
                        ioManager.setOutput(index, state);
                        ioManager.updateOutputs();
                        server.send(200, "text/plain", "OK");
                } else {
                        server.send(400, "text/plain", "Index error");
                }
        });

        server.on("/program", HTTP_GET, [this]() {
                server.sendHeader("Access-Control-Allow-Origin", "*");
                String json = ladderEngine.serializeProgram();
                server.sendHeader("Content-Type", "application/json");
                server.send(200, "application/json", json);
        });

        server.on("/program", HTTP_POST, [this]() {
                server.sendHeader("Access-Control-Allow-Origin", "*");
                if (!server.hasArg("plain")) { 
                        Serial.println("[HTTP] POST /program: sem body");
                        server.send(400, "text/plain", "Bad Request"); 
                        return; 
                }
                String body = server.arg("plain");
                Serial.printf("[HTTP] POST /program: %d bytes\n", body.length());
                if (ladderEngine.loadFromJson(body, true)) {
                        server.send(200, "text/plain", "Programa salvo e carregado");
                        Serial.println("[HTTP] Programa salvo OK");
                } else {
                        server.send(400, "text/plain", "Programa inválido");
                        Serial.println("[HTTP] Programa inválido");
                }
        });

        server.on("/program", HTTP_DELETE, [this]() {
                server.sendHeader("Access-Control-Allow-Origin", "*");
                ladderEngine.eraseStorage();
                server.send(200, "text/plain", "Programa apagado");
        });

        server.on("/cycle", HTTP_POST, [this]() {
                server.sendHeader("Access-Control-Allow-Origin", "*");
                if (!server.hasArg("plain")) { server.send(400, "text/plain", "Bad Request"); return; }
                StaticJsonDocument<64> doc;
                if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "JSON error"); return; }
                uint16_t ms = doc["ms"] | ladderEngine.getCycleMs();
                ladderEngine.setCycleMs(ms);
                server.send(200, "text/plain", "Cycle updated");
        });

        server.onNotFound([this]() {
                server.sendHeader("Access-Control-Allow-Origin", "*");
                server.send(404, "text/plain", "Not found");
        });
}

void ServerManager::begin() {
        server.begin();
        Serial.println("[HTTP] Servidor na porta 80");
}

void ServerManager::handleClient() {
        server.handleClient();
}
