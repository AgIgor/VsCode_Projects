#include "web_control_server.h"

#include <FS.h>
#include <SPIFFS.h>

WebControlServer::WebControlServer(ScriptVM& vm)
    : server_(80), vm_(vm), script_("") {}

void WebControlServer::begin(const char* apSsid, const char* apPassword) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid, apPassword);

  SPIFFS.begin(true);
  if (!loadScriptFromStorage()) {
    script_ = "# Exemplo\n"
              "ON 2\n"
              "WAIT 1000\n"
              "OFF 2\n"
              "COUNT ciclos INC 1\n";
  }

  String error;
  vm_.loadScript(script_, error);

  setupRoutes();
  server_.begin();
}

void WebControlServer::loop() {
  server_.handleClient();
}

IPAddress WebControlServer::ip() const {
  return WiFi.softAPIP();
}

void WebControlServer::setupRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/script", HTTP_GET, [this]() { handleGetScript(); });
  server_.on("/api/script", HTTP_POST, [this]() { handleSetScript(); });
  server_.on("/api/run", HTTP_POST, [this]() { handleRun(); });
  server_.on("/api/stop", HTTP_POST, [this]() { handleStop(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });

  server_.onNotFound([this]() {
    server_.send(404, "application/json", "{\"error\":\"not_found\"}");
  });
}

bool WebControlServer::loadScriptFromStorage() {
  if (!SPIFFS.exists("/script.txt")) {
    return false;
  }

  File file = SPIFFS.open("/script.txt", FILE_READ);
  if (!file) {
    return false;
  }

  script_ = file.readString();
  file.close();
  return !script_.isEmpty();
}

bool WebControlServer::saveScriptToStorage(const String& script) {
  File file = SPIFFS.open("/script.txt", FILE_WRITE);
  if (!file) {
    return false;
  }

  const size_t written = file.print(script);
  file.close();
  return written == script.length();
}

void WebControlServer::handleRoot() {
  static const char page[] PROGMEM = R"HTML(
<!doctype html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>ESP32 Script Runtime</title>
  <style>
    :root {
      --bg0: #f5f2ea;
      --bg1: #e7dcc6;
      --ink: #1c1f1a;
      --accent: #17624a;
      --accent-2: #d6852d;
      --card: #fffaf0;
      --line: #d2c6ae;
    }
    body {
      margin: 0;
      font-family: "Trebuchet MS", "Verdana", sans-serif;
      color: var(--ink);
      background: radial-gradient(circle at top right, var(--bg1), var(--bg0) 45%);
      min-height: 100vh;
      padding: 16px;
    }
    .wrap {
      max-width: 980px;
      margin: 0 auto;
      background: var(--card);
      border: 1px solid var(--line);
      border-radius: 14px;
      padding: 14px;
      box-shadow: 0 8px 30px rgba(0,0,0,0.09);
    }
    h1 {
      margin: 0 0 8px;
      font-size: 1.2rem;
      letter-spacing: 0.4px;
    }
    .grid {
      display: grid;
      grid-template-columns: 1fr;
      gap: 10px;
    }
    textarea {
      width: 100%;
      min-height: 320px;
      font-family: "Consolas", monospace;
      font-size: 14px;
      border: 1px solid var(--line);
      border-radius: 10px;
      background: #fffdf8;
      padding: 10px;
      box-sizing: border-box;
    }
    .row {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
    }
    button {
      border: 0;
      border-radius: 10px;
      padding: 10px 14px;
      cursor: pointer;
      color: #fff;
      background: var(--accent);
      font-weight: 700;
      transition: transform .15s ease;
    }
    button.alt { background: #7a2f2f; }
    button.warn { background: var(--accent-2); color: #1f1405; }
    button:hover { transform: translateY(-1px); }
    pre {
      margin: 0;
      background: #1b1f24;
      color: #e9f1ff;
      padding: 10px;
      border-radius: 10px;
      min-height: 80px;
      overflow: auto;
    }
    .help {
      font-size: 13px;
      line-height: 1.4;
      background: #f8f2e4;
      border: 1px solid var(--line);
      border-radius: 10px;
      padding: 8px;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <h1>ESP32 Script Runtime</h1>
    <div class="grid">
      <textarea id="script"></textarea>
      <div class="row">
        <button id="save">Salvar Script</button>
        <button id="run" class="warn">Executar</button>
        <button id="stop" class="alt">Parar</button>
        <button id="refresh">Status</button>
      </div>
      <div class="help">
        Comandos:\nON pin\nOFF pin\nWAIT ms\nCOUNT nome INC [valor]\nCOUNT nome DEC [valor]\nCOUNT nome SET valor\nCOUNT nome RESET
      </div>
      <pre id="out"></pre>
    </div>
  </div>
  <script>
    const scriptEl = document.getElementById('script');
    const outEl = document.getElementById('out');

    const show = (msg) => { outEl.textContent = msg; };

    async function loadScript() {
      const r = await fetch('/api/script');
      scriptEl.value = await r.text();
    }

    async function saveScript() {
      const r = await fetch('/api/script', { method: 'POST', body: scriptEl.value });
      show(await r.text());
    }

    async function runScript() {
      const r = await fetch('/api/run', { method: 'POST' });
      show(await r.text());
      await getStatus();
    }

    async function stopScript() {
      const r = await fetch('/api/stop', { method: 'POST' });
      show(await r.text());
      await getStatus();
    }

    async function getStatus() {
      const r = await fetch('/api/status');
      show(JSON.stringify(await r.json(), null, 2));
    }

    document.getElementById('save').onclick = saveScript;
    document.getElementById('run').onclick = runScript;
    document.getElementById('stop').onclick = stopScript;
    document.getElementById('refresh').onclick = getStatus;

    loadScript().then(getStatus);
    setInterval(getStatus, 1000);
  </script>
</body>
</html>
)HTML";

  server_.send(200, "text/html", page);
}

void WebControlServer::handleGetScript() {
  server_.send(200, "text/plain; charset=utf-8", script_);
}

void WebControlServer::handleSetScript() {
  const String incoming = server_.arg("plain");
  if (incoming.isEmpty()) {
    server_.send(400, "application/json", "{\"error\":\"script_vazio\"}");
    return;
  }

  String error;
  if (!vm_.loadScript(incoming, error)) {
    String safeError = error;
    safeError.replace("\\", "\\\\");
    safeError.replace("\"", "\\\"");
    String payload = "{\"error\":\"" + safeError + "\"}";
    server_.send(400, "application/json", payload);
    return;
  }

  script_ = incoming;
  saveScriptToStorage(script_);
  server_.send(200, "application/json", "{\"ok\":true,\"message\":\"script_salvo\"}");
}

void WebControlServer::handleRun() {
  vm_.start();
  server_.send(200, "application/json", "{\"ok\":true,\"message\":\"executando\"}");
}

void WebControlServer::handleStop() {
  vm_.stop();
  server_.send(200, "application/json", "{\"ok\":true,\"message\":\"parado\"}");
}

void WebControlServer::handleStatus() {
  server_.send(200, "application/json", vm_.getStatusJson());
}
