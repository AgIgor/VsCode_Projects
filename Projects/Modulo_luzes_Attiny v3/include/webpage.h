#ifndef WEBPAGE_H
#define WEBPAGE_H

// Página HTML/CSS/JavaScript armazenada em PROGMEM
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Módulo de Luzes</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 10px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
            max-width: 600px;
            width: 100%;
            padding: 30px;
        }
        h1 {
            color: #333;
            margin-bottom: 10px;
            text-align: center;
        }
        .status {
            text-align: center;
            color: #666;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .section {
            margin-bottom: 30px;
            padding-bottom: 30px;
            border-bottom: 1px solid #eee;
        }
        .section:last-child {
            border-bottom: none;
        }
        .section h2 {
            color: #667eea;
            font-size: 18px;
            margin-bottom: 20px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            color: #333;
            margin-bottom: 8px;
            font-weight: 500;
        }
        input[type="number"], input[type="text"] {
            width: 100%;
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 5px;
            font-size: 14px;
            transition: border-color 0.3s;
        }
        input[type="number"]:focus, input[type="text"]:focus {
            outline: none;
            border-color: #667eea;
        }
        .input-unit {
            color: #999;
            font-size: 12px;
            margin-top: 5px;
        }
        .checkbox-group {
            display: flex;
            flex-direction: column;
            gap: 15px;
        }
        .checkbox-item {
            display: flex;
            align-items: center;
            padding: 10px;
            background: #f8f9fa;
            border-radius: 5px;
        }
        input[type="checkbox"] {
            width: 20px;
            height: 20px;
            cursor: pointer;
            margin-right: 10px;
        }
        .checkbox-item label {
            margin: 0;
            cursor: pointer;
            flex: 1;
        }
        .button-group {
            display: flex;
            gap: 10px;
            margin-top: 30px;
        }
        button {
            flex: 1;
            padding: 12px;
            border: none;
            border-radius: 5px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
        }
        .btn-save {
            background: #667eea;
            color: white;
        }
        .btn-save:hover {
            background: #5568d3;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
        }
        .btn-reset {
            background: #f0f0f0;
            color: #333;
        }
        .btn-reset:hover {
            background: #e0e0e0;
        }
        .message {
            padding: 12px;
            border-radius: 5px;
            margin-bottom: 20px;
            display: none;
        }
        .message.success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .message.error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .info-box {
            background: #e7f3ff;
            border-left: 4px solid #2196F3;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
            font-size: 13px;
            color: #0c5aa0;
        }
        .loading {
            display: none;
            text-align: center;
            color: #667eea;
            font-weight: 500;
        }
        .spinner {
            border: 3px solid #f3f3f3;
            border-top: 3px solid #667eea;
            border-radius: 50%;
            width: 20px;
            height: 20px;
            animation: spin 1s linear infinite;
            display: inline-block;
            margin-right: 10px;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>⚡ Módulo de Luzes</h1>
        <div class="status">
            <span id="deviceStatus">Conectando...</span>
        </div>

        <div id="message" class="message"></div>
        <div class="loading" id="loading"><span class="spinner"></span>Salvando...</div>

        <!-- SEÇÃO: INFORMAÇÕES -->
        <div class="info-box">
            Todos os tempos em <strong>milissegundos (ms)</strong>. Configurações são salvas na memória do ESP32.
        </div>

        <!-- SEÇÃO: FOLLOW-ME -->
        <div class="section">
            <h2>🚗 Follow-Me (Ignição Desligada)</h2>
            <div class="form-group">
                <label for="followMe1">Follow-Me com 1 Piscada (ms)</label>
                <input type="number" id="followMe1" min="1000" max="120000" step="1000">
                <div class="input-unit">Padrão: 40000 ms (40s)</div>
            </div>
            <div class="form-group">
                <label for="followMe2">Follow-Me com 2+ Piscadas (ms)</label>
                <input type="number" id="followMe2" min="1000" max="120000" step="1000">
                <div class="input-unit">Padrão: 25000 ms (25s)</div>
            </div>
            <div class="form-group">
                <label for="janelaP">Janela de Detecção de Piscadas (ms)</label>
                <input type="number" id="janelaP" min="200" max="5000" step="100">
                <div class="input-unit">Padrão: 1200 ms</div>
            </div>
            <div class="checkbox-group">
                <div class="checkbox-item">
                    <input type="checkbox" id="ativarFollowMe">
                    <label for="ativarFollowMe">💡 Ativar Follow-Me (piscadas de seta)</label>
                </div>
            </div>
        </div>

        <!-- SEÇÃO: IGNIÇÃO -->
        <div class="section">
            <h2>🔑 Controle de Ignição</h2>
            <div class="form-group">
                <label for="posPosIgnicao">Tempo Pós-Ignição (ms)</label>
                <input type="number" id="posPosIgnicao" min="1000" max="60000" step="1000">
                <div class="input-unit">Farol aceso após desligar ignição. Padrão: 10000 ms (10s)</div>
            </div>
            <div class="checkbox-group">
                <div class="checkbox-item">
                    <input type="checkbox" id="ativarLuzPosPosChave">
                    <label for="ativarLuzPosPosChave">🔦 Ativar luzes ao ligar a chave</label>
                </div>
                <div class="checkbox-item">
                    <input type="checkbox" id="ativarDrlPosChave">
                    <label for="ativarDrlPosChave">💡 Ativar DRL após desligar a chave</label>
                </div>
            </div>
        </div>

        <!-- SEÇÃO: TRAVA/DESTRAVA -->
        <div class="section">
            <h2>🔐 Trava/Destrava (ESP32)</h2>
            <div class="form-group">
                <label for="tempoLuzTrava">Tempo de Luz ao Trancar/Destrancar (ms)</label>
                <input type="number" id="tempoLuzTrava" min="500" max="30000" step="500">
                <div class="input-unit">Quanto tempo o farol fica ligado. Padrão: 3000 ms (3s)</div>
            </div>
            <div class="checkbox-group">
                <div class="checkbox-item">
                    <input type="checkbox" id="ativarLuzTrava">
                    <label for="ativarLuzTrava">💡 Ativar luzes ao trancar/destrancar</label>
                </div>
            </div>
        </div>

        <!-- SEÇÃO: RELÉ DAS SETAS -->
        <div class="section">
            <h2>🔔 Relé das Setas</h2>
            <div class="form-group">
                <label for="tempoMinRele">Tempo Mínimo do Relé (ms)</label>
                <input type="number" id="tempoMinRele" min="500" max="10000" step="100">
                <div class="input-unit">Relé permanece ligado por no mínimo. Padrão: 2500 ms</div>
            </div>
            <div class="form-group">
                <label for="tempoSetaInativa">Tempo para Detectar Seta Parada (ms)</label>
                <input type="number" id="tempoSetaInativa" min="500" max="10000" step="100">
                <div class="input-unit">Padrão: 3000 ms</div>
            </div>
        </div>

        <!-- SEÇÃO: DEBOUNCE -->
        <div class="section">
            <h2>⚙️ Ajustes Avançados</h2>
            <div class="form-group">
                <label for="debounce">Debounce (ms)</label>
                <input type="number" id="debounce" min="5" max="100" step="5">
                <div class="input-unit">Intervalo de leitura de entradas. Padrão: 20 ms</div>
            </div>
            <div class="form-group">
                <label for="leitura">Intervalo de Leitura do Loop (ms)</label>
                <input type="number" id="leitura" min="50" max="500" step="50">
                <div class="input-unit">Padrão: 150 ms</div>
            </div>
        </div>

        <!-- BOTÕES -->
        <div class="button-group">
            <button class="btn-save" onclick="salvarConfigs()">💾 Salvar Configurações</button>
            <button class="btn-reset" onclick="carregarConfigs()">🔄 Carregar Padrão</button>
        </div>
    </div>

    <script>
        const API_URL = 'http://' + window.location.hostname;

        // Carregar configurações ao abrir a página
        window.addEventListener('load', function() {
            carregarConfigs();
            atualizarStatus();
        });

        // Atualizar status a cada 5 segundos
        setInterval(atualizarStatus, 5000);

        function atualizarStatus() {
            fetch(API_URL + '/api/status')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('deviceStatus').textContent = 
                        '✅ Conectado ao: ' + data.ssid + ' (IP: ' + data.ip + ')';
                })
                .catch(e => {
                    document.getElementById('deviceStatus').textContent = '❌ Desconectado';
                });
        }

        function carregarConfigs() {
            fetch(API_URL + '/api/config')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('followMe1').value = data.followMe1 || 40000;
                    document.getElementById('followMe2').value = data.followMe2 || 25000;
                    document.getElementById('janelaP').value = data.janelaP || 1200;
                    document.getElementById('posPosIgnicao').value = data.posPosIgnicao || 10000;
                    document.getElementById('tempoMinRele').value = data.tempoMinRele || 2500;
                    document.getElementById('tempoSetaInativa').value = data.tempoSetaInativa || 3000;
                    document.getElementById('debounce').value = data.debounce || 20;
                    document.getElementById('leitura').value = data.leitura || 150;
                    document.getElementById('tempoLuzTrava').value = data.tempoLuzTrava || 3000;
                    document.getElementById('ativarLuzTrava').checked = data.ativarLuzTrava !== false;
                    document.getElementById('ativarLuzPosPosChave').checked = data.ativarLuzPosPosChave !== false;
                    document.getElementById('ativarFollowMe').checked = data.ativarFollowMe !== false;
                    document.getElementById('ativarDrlPosChave').checked = data.ativarDrlPosChave !== false;
                    mostrarMensagem('✅ Configurações carregadas', 'success');
                })
                .catch(e => {
                    mostrarMensagem('❌ Erro ao carregar configurações', 'error');
                    console.error(e);
                });
        }

        function salvarConfigs() {
            const loading = document.getElementById('loading');
            loading.style.display = 'block';

            const config = {
                followMe1: parseInt(document.getElementById('followMe1').value),
                followMe2: parseInt(document.getElementById('followMe2').value),
                janelaP: parseInt(document.getElementById('janelaP').value),
                posPosIgnicao: parseInt(document.getElementById('posPosIgnicao').value),
                tempoMinRele: parseInt(document.getElementById('tempoMinRele').value),
                tempoSetaInativa: parseInt(document.getElementById('tempoSetaInativa').value),
                debounce: parseInt(document.getElementById('debounce').value),
                leitura: parseInt(document.getElementById('leitura').value),
                tempoLuzTrava: parseInt(document.getElementById('tempoLuzTrava').value),
                ativarLuzTrava: document.getElementById('ativarLuzTrava').checked,
                ativarLuzPosPosChave: document.getElementById('ativarLuzPosPosChave').checked,
                ativarFollowMe: document.getElementById('ativarFollowMe').checked,
                ativarDrlPosChave: document.getElementById('ativarDrlPosChave').checked
            };

            fetch(API_URL + '/api/config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(config)
            })
            .then(response => response.json())
            .then(data => {
                loading.style.display = 'none';
                if (data.success) {
                    mostrarMensagem('✅ Configurações salvas com sucesso!', 'success');
                } else {
                    mostrarMensagem('❌ Erro ao salvar: ' + data.error, 'error');
                }
            })
            .catch(e => {
                loading.style.display = 'none';
                mostrarMensagem('❌ Erro na conexão: ' + e.message, 'error');
                console.error(e);
            });
        }

        function mostrarMensagem(texto, tipo) {
            const msg = document.getElementById('message');
            msg.textContent = texto;
            msg.className = 'message ' + tipo;
            msg.style.display = 'block';
            setTimeout(() => {
                msg.style.display = 'none';
            }, 5000);
        }
    </script>
</body>
</html>
)rawliteral";

#endif
