# 🚗 Automatizador de Luzes Automotivo - ESP32

Sistema de automação de luzes automotivas com interface web de configuração, adaptado para **ESP32**.

## ✨ Recursos

- **Interface Web Responsiva**: Painel de controle completo via navegador
- **Configuração Remota**: Modifique tempos, pinos e parâmetros sem recompilação
- **Armazenamento Persistente**: Configurações salvas em LittleFS
- **Múltiplos Modos de Operação**:
  - 🌙 **Auto Drive**: Farol automático em ambiente escuro
  - 🚪 **Leaving Home**: Iluminação ao trancar/destravar
  - 🔓 **Cortesia de Porta**: Iluminação ao abrir porta
  - 🏠 **Follow Me Home**: Luz após desligar ignição
- **Monitoramento em Tempo Real**: Status de entradas, saídas e sensores
- **API REST**: Endpoints para integração externa

## 🛠️ Hardware Necessário

- **ESP32** (DevKit V1 ou similar)
- **Sensores/Entradas**:
  - 5x Entradas digitais (trava, seta, porta, ignição, destrava)
  - 1x Entrada analógica ADC (sensor de luz LDR)
- **Saídas**:
  - 2x Saídas digitais (farol, acessório)

## 📦 Instalação

### 1. Dependências do PlatformIO

O `platformio.ini` já está configurado com as bibliotecas necessárias:
- ESP32 WebServer
- ArduinoJson
- ESPAsyncWebServer
- AsyncTCP
- LittleFS

### 2. Clonar/Abrir o Projeto

```bash
# No VS Code com PlatformIO
platformio run --target upload  # Compilar e carregar
```

### 3. Primeira Inicialização

Ao ligar o ESP32:
1. Acesse a rede WiFi: **LuzesAuto** (senha: 12345678)
2. Abra no navegador: **http://192.168.4.1** ou **http://luz-auto.local**
3. Configure seus pinos e parâmetros conforme necessário

## 📋 Abas de Configuração

### ⏱️ Tempos
- **Debounce**: Tempo de estabilização dos sinais (~30ms)
- **Signal Pair Window**: Reconhecimento de sinais duplos (~1.5s)
- **Leaving Home**: Duração da iluminação ao trancar/destravar (~20-40s)
- **Cortesia de Porta**: Duração da iluminação ao abrir porta (~20s)
- **Follow Me Home**: Duração após desligar ignição (~30s)
- **Acessório Ligado**: Tempo do modo acessório (~10 minutos)

### 📥 Entradas (Inputs)
Configure o pino GPIO e a lógica de inversão para cada entrada:
- **Trava** (default GPIO 4)
- **Seta** (default GPIO 5)
- **Porta** (default GPIO 6)
- **Ignição** (default GPIO 7)
- **Destrava** (default GPIO 8)
- **LDR/Sensor de Luz** (default GPIO 35 - ADC)

### 📤 Saídas (Outputs)
Configure o pino GPIO para cada saída:
- **Farol** (default GPIO 2)
- **Acessório** (default GPIO 3)

### 💡 Sensor de Luz (LDR)
- **Limiar de Escuridão**: Valor ADC considerado escuro (0-4095)
- **Limiar de Claridade**: Valor ADC considerado claro
- **Detecção de Falhas**: Valores extremos para detectar sensor aberto/curto
- **Valor Atual**: Monitoramento em tempo real

### ⚙️ Features
Ativar/desativar modos de operação:
- Auto Drive
- Leaving Home
- Cortesia de Porta
- Follow Me Home

### 🔧 Sistema
- Informações do firmware
- Reset para configurações padrões
- Recarregar página

## 🌐 API REST

A interface web usa estes endpoints:

### GET `/api/config`
Obtém configuração atual em JSON

```bash
curl http://192.168.4.1/api/config
```

### POST `/api/config`
Atualiza configuração

```bash
curl -X POST http://192.168.4.1/api/config \
  -H "Content-Type: application/json" \
  -d '{"debounceMs": 50, "leavingHomeLockMs": 45000}'
```

### GET `/api/status`
Status em tempo real: inputs, outputs, uptime, memória livre

```bash
curl http://192.168.4.1/api/status
```

### POST `/api/reset`
Resetar para configurações padrões

```bash
curl -X POST http://192.168.4.1/api/reset
```

## 📁 Estrutura do Projeto

```
Automatizador_Luzes_Automotivo/
├── include/
│   ├── PinConfig.h       # Configuração de pinos
│   ├── ConfigManager.h   # Gerenciamento de config (JSON/LittleFS)
│   └── WebServer.h       # Servidor web + interface HTML
├── src/
│   └── main.cpp          # Lógica principal
├── platformio.ini        # Configuração do projeto
└── README.md            # Este arquivo
```

## 🔌 Mapeamento de Pinos (Padrão)

| Função | GPIO | Tipo | Notas |
|--------|------|------|-------|
| Trava | 4 | Input | Pull-up, invertido |
| Seta | 5 | Input | Pull-up, invertido |
| Porta | 6 | Input | Pull-up |
| Ignição | 7 | Input | Pull-up, invertido |
| Destrava | 8 | Input | Pull-up, invertido |
| LDR | 35 | ADC | Sensor de luz |
| Farol | 2 | Output | Ativo alto |
| Acessório | 3 | Output | Ativo alto |

**Dica**: Você pode modificar todos esses valores via interface web!

## 🚀 Como Compilar e Carregar

### Via VS Code + PlatformIO

1. Abra o projeto no VS Code com extensão PlatformIO
2. Conecte o ESP32 via USB
3. Clique em "Upload" (✓) na barra de status
4. Aguarde a compilação e upload

### Via Terminal

```bash
# Compilar apenas
platformio run

# Compilar e fazer upload
platformio run --target upload

# Monitorar saída serial
platformio device monitor
```

## 📊 Monitoramento Serial

A saída serial mostra status do sistema a cada 5 segundos:

```
=== System Status ===
Uptime: 125 seconds
--- Inputs ---
Lock: HIGH
Turn Signal: LOW
Door: LOW
Ignition: HIGH
Unlock: HIGH
LDR Value: 852
--- Outputs ---
Headlight: ON
Accessory: OFF
Free Heap: 204.5 KB
====================
```

## 🔒 WiFi & Segurança

- **SSID**: LuzesAuto
- **Senha**: 12345678
- **IP**: 192.168.4.1
- **Hostname**: luz-auto.local

⚠️ **Nota de Segurança**: Altere a senha WiFi no código se usar em produção!

```cpp
// Em PinConfig.h
const char* WIFI_PASSWORD = "sua_senha_forte_aqui";
```

## 📝 Configuração de Exemplo

### Montagem Básica (4 Entradas + 1 Saída)

Para um sistema simplificado, você pode usar:
- GPIO 4: Porta (entrada)
- GPIO 5: Ignição (entrada)
- GPIO 35: LDR (entrada analógica)
- GPIO 2: Farol (saída PWM/Digital)

### Uso Avançado (Todas as Entradas + 2 Saídas)

Configure conforme o hardware disponível na interface web.

## 🐛 Troubleshooting

### Erro: "LittleFS not found"
- Certifique-se de que `platformio.ini` tem: `board_build.filesystem = littlefs`
- Recompile do zero: `platformio run --target clean`

### WiFi não conecta
- Verifique se a biblioteca `AsyncTCP` está instalada
- Reset do ESP32: segure RESET por 2 segundos

### Interface web não carrega
- Tente acessar: `http://192.168.4.1`
- Verifique o console do navegador (F12) para erros
- Reset do ESP32 e tente novamente

### Pinos não respondem
- Verifique o GPIO mapeado via interface web
- Confirme que o GPIO está disponível no ESP32 (não é GND, VDD, etc.)
- Teste com digitalWrite() no serial monitor

## 📚 Documentação de Desenvolvimento

O código é modular:
- **PinConfig.h**: Define pinos e constantes
- **ConfigManager.h**: JSON + LittleFS, carrega/salva configurações
- **WebServer.h**: AsyncWebServer + endpoints da API + HTML inline
- **main.cpp**: Lógica de controle principal, loop de leitura de inputs

Para adicionar novos parâmetros:
1. Adicione em `SystemConfig` (ConfigManager.h)
2. Carregue/salve em `loadFromFile()` / `saveToFile()`
3. Exponha na API GET `/api/config`
4. Adicione campo no HTML e JavaScript

## 📄 Licença

Projeto aberto para uso e modificação. Sinta-se livre para adaptar!

## 🎯 Próximas Melhorias Sugeridas

- [ ] Suporte a múltiplas saídas PWM para intensidade
- [ ] Histórico de eventos
- [ ] Modo sleep para economia de energia
- [ ] MQTT para integração IoT
- [ ] Certificado SSL/HTTPS
- [ ] Autenticação com login
- [ ] Dashboard de estatísticas

---

**Desenvolvido para ESP32** 🎉  
Para dúvidas ou sugestões, modifique o código conforme necessário!
