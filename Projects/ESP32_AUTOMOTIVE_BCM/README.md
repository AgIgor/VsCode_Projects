# ESP32 Automotive BCM - Body Control Module

Sistema de controle automotivo baseado em ESP32 para gerenciamento inteligente de iluminação.

## 📋 Funcionalidades

- **Interface Web Intuitiva**: Configure lógicas de iluminação através de navegador
- **Modo Access Point**: ESP32 cria rede WiFi própria (BCM_ESP32)
- **4 Entradas Digitais** (Pull-up):
  - Ignição (GPIO 13)
  - Porta (GPIO 14)
  - Destrava (GPIO 25)
  - Trava (GPIO 26)
- **4 Saídas Digitais**:
  - Farol (GPIO 16)
  - DRL - Daytime Running Lights (GPIO 17)
  - Luz Interna (GPIO 18)
  - Luz dos Pés (GPIO 19)
- **Lógica Condicional**: Ações baseadas em eventos, estado de porta e ignição
- **Temporização**: Ligar/desligar saídas após delay configurável (0-300s)
- **Persistência**: Configurações salvas em LittleFS
- **Sistema Fail-Safe**: Desliga todas as saídas após 10 minutos com ignição desligada (proteção de bateria)

## 🔧 Hardware

### Pinout
```
ENTRADAS (INPUT_PULLUP - Ativo em LOW):
├─ GPIO 13 → Ignição
├─ GPIO 14 → Porta
├─ GPIO 25 → Destrava
└─ GPIO 26 → Trava

SAÍDAS (OUTPUT - HIGH = ligado):
├─ GPIO 16 → Farol
├─ GPIO 17 → DRL
├─ GPIO 18 → Luz Interna
└─ GPIO 19 → Luz dos Pés
```

### Esquema de Ligação
- **Entradas**: Conectar botões/sensores entre GPIO e GND (pull-up interno ativado)
- **Saídas**: Conectar através de driver (transistor/MOSFET/relé) para cargas maiores
  - ⚠️ **Atenção**: ESP32 fornece máximo 12mA por pino (3.3V)
  - Recomendado: ULN2803 ou MOSFETs para controlar relés/lâmpadas

## 🚀 Instalação

### 1. Dependências
```bash
pip install platformio
```

### 2. Compilar e Upload do Firmware
```bash
cd ESP32_AUTOMOTIVE_BCM
pio run -e esp32doit-devkit-v1 --target upload
```

### 3. Upload do Sistema de Arquivos (Interface Web)

**Método Automático** (com plugin):
```bash
pio run -e esp32doit-devkit-v1 --target uploadfs
```

**Método Manual** (se plugin não estiver instalado):
1. Instalar `mklittlefs`:
   ```bash
   # Windows (baixar de https://github.com/earlephilhower/mklittlefs/releases)
   # Ou usar ESP32FS plugin para Arduino IDE
   ```

2. Criar imagem LittleFS:
   ```bash
   mklittlefs -c data -s 0x170000 littlefs.bin
   ```

3. Flash via esptool:
   ```bash
   esptool.py --chip esp32 --port COM3 write_flash 0x290000 littlefs.bin
   ```

**Método Alternativo** (sem filesystem upload):
- Acesse `http://192.168.4.1` → Verá erro "index.html não encontrado"
- Use API diretamente ou reconfigure código para servir HTML from PROGMEM

## 📡 Uso

### 1. Conectar ao ESP32
1. Ligue o ESP32
2. Conecte-se à rede WiFi: **BCM_ESP32**
3. Senha: **12345678**
4. Acesse: **http://192.168.4.1**

### 2. Configurar Lógicas
1. Clique em **"➕ Adicionar Lógica"**
2. Selecione o **evento** disparador:
   - `destravou` / `travou`
   - `ignicao_on` / `ignicao_off`
   - `porta_abriu` / `porta_fechou`
3. Configure **condições opcionais**:
   - Estado da porta (qualquer/aberta/fechada)
   - Estado da ignição (qualquer/ligada/desligada)
4. Para cada saída:
   - ✅ Ative o controle
   - **Ligar após**: segundos até acionar (0 = desativado)
   - **Desligar após**: segundos até desligar (0 = desativado)
5. Clique em **"💾 Salvar Tudo"**

### 3. Exemplos de Lógica

**Luz de Cortesia (Porta Aberta)**:
- Evento: `porta_abriu`
- Condição: Ignição = `desligada`
- Saída `interna`: ligar após 0s, desligar após 30s

**DRL Automático**:
- Evento: `ignicao_on`
- Saída `drl`: ligar após 2s, desligar após 0s (mantém ligado)

**Farol ao Destravar**:
- Evento: `destravou`
- Condição: Porta = `fechada`
- Saída `farol`: ligar após 1s, desligar após 60s

## 📊 API REST

### GET /api/config
Retorna configuração atual:
```json
{
  "logicas": [
    {
      "evento": "destravou",
      "conditions": {
        "porta": "any",
        "ignicao": "off"
      },
      "outputs": {
        "farol": {
          "enabled": true,
          "ligar": {"after": 1},
          "desligar": {"after": 60}
        }
      }
    }
  ]
}
```

### POST /api/config
Salva nova configuração (envia JSON acima)

### GET /api/status
Estado atual do sistema:
```json
{
  "uptime": 3600,
  "freeHeap": 180000,
  "inputs": {
    "ignicao": false,
    "porta": true,
    "trava": false,
    "destrava": false
  },
  "outputs": {
    "farol": 1,
    "drl": 0,
    "interna": 1,
    "pes": 0
  }
}
```

### POST /api/reset
Apaga todas as configurações

## 🔍 Debug

### Monitor Serial
```bash
pio device monitor -b 115200
```

**Saída Esperada**:
```
=== ESP32 BCM Automotive ===
LittleFS montado com sucesso
Configuração carregada: {...}
AP iniciado: BCM_ESP32
Endereço IP: 192.168.4.1
Servidor HTTP iniciado
Acesse: http://192.168.4.1

[Evento] porta_abriu detectado
  Condições atendidas, processando saídas:
    interna: ligar após 0s
    interna: desligar após 30s
✓ interna LIGADO
[30000ms] ✗ interna DESLIGADO
```

## �️ Sistema Fail-Safe (Proteção de Bateria)

O sistema possui proteção automática contra descarga de bateria:

- **Timer de 10 minutos**: Ativado quando a ignição é desligada
- **Desligamento automático**: Todas as saídas são desligadas após o timeout
- **Reset ao ligar ignição**: O timer é cancelado quando a ignição é ligada novamente
- **Proteção contra falhas**: Evita que luzes permaneçam acesas indefinidamente

### Logs do Fail-Safe
```
Evento detectado: ignicao_off
⏱️  Ignição desligada - fail-safe ativo em 600 segundos

[Após 10 minutos sem ligar ignição...]
⚠️  FAIL-SAFE ATIVADO: Desligando todas as saídas!
  ✗ farol desligado
  ✗ drl desligado
  ✗ interna desligado
  ✗ pes desligado

[Ao ligar ignição novamente...]
Evento detectado: ignicao_on
✓ Ignição ligada - fail-safe desativado
```

### Configuração do Timeout
Para ajustar o tempo (padrão: 600 segundos), edite em `src/main.cpp`:
```cpp
#define TIMEOUT_IGNICAO_OFF 600000  // 10 minutos em ms
```

## �🛠️ Desenvolvimento

### Estrutura de Arquivos
```
ESP32_AUTOMOTIVE_BCM/
├── platformio.ini       # Configuração PlatformIO
├── src/
│   └── main.cpp         # Firmware principal
├── data/
│   └── index.html       # Interface web
├── include/
├── lib/
└── README.md
```

### Bibliotecas Utilizadas
- **ArduinoJson** 7.0+: Parsing JSON
- **ESPAsyncWebServer**: Servidor HTTP assíncrono
- **AsyncTCP**: Comunicação TCP não-bloqueante
- **LittleFS**: Sistema de arquivos

### Modificar Credenciais WiFi
Edite `src/main.cpp`:
```cpp
#define AP_SSID "SEU_NOME_REDE"
#define AP_PASSWORD "SUA_SENHA_8_CHARS"
```

### Customizar Pins
Edite defines no `main.cpp`:
```cpp
#define PIN_IGNICAO 13
// ... etc
```

## ⚠️ Avisos

1. **Segurança Elétrica**: 
   - Use drivers adequados para cargas >12mA
   - Proteja circuito com fusíveis
   - Isole adequadamente conexões

2. **Automotivo**:
   - Ruído elétrico de partida pode resetar ESP32
   - Considere capacitores de filtro na alimentação
   - Proteja contra inversão de polaridade

3. **WiFi**:
   - Alcance limitado (10-30m)
   - Rede aberta após resetar senha padrão
   - Para produção: implemente autenticação robusta

## 📝 Licença

MIT License - Use livremente, por sua conta e risco.

## 🤝 Contribuições

Pull requests são bem-vindos! Para mudanças importantes, abra uma issue primeiro.

---

**Desenvolvido com ❤️ para entusiastas automotivos**
