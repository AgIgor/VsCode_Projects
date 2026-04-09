# ESP32 RealDash Simulator (ELM327 + Qt/QML)

Este projeto cria um simulador para o RealDash com duas partes:

1. **ESP32 (PlatformIO)**: emula um adaptador **ELM327** via Bluetooth SPP (`ESP32-ELM327`).
2. **App desktop Qt/QML**: envia telemetria em tempo real para o ESP32 via serial USB com simulação automática de conduções realistas.

## ⚡ Quick Start (Windows)

1. Dentro da pasta `python-realdash-sender/`, double-click em **`RUN.bat`**
2. Ou no terminal (PowerShell/CMD):

```powershell
cd python-realdash-sender
python -m pip install -r requirements.txt
python main.py
```

## 1) Firmware ESP32

Arquivo principal: `src/main.cpp`

### O que ele faz
- Lê a telemetria recebida na serial USB em formato texto.
- Responde comandos AT e PIDs OBD2 (modo 01) pelo Bluetooth.
- Permite o RealDash conectar via perfil ELM327.

### Formato serial aceito
Envie linhas como:

RPM=1500,SPEED=40,ECT=86,TPS=20,MAP=45,VBAT=13.80,FUEL=70

Também aceita `;` no lugar de `,`.

Formato completo suportado:

RPM=1500,SPEED=40,LOAD=25,ECT=86,IAT=33,TPS=20,MAP=45,MAF=6.20,ADV=12,VBAT=13.80,FUEL=70,RUNTIME=321,OILTEMP=92

### PIDs implementados
- `0100`, `0120`, `0140` (máscaras de suporte)
- `0104` carga do motor
- `010C` RPM
- `010D` velocidade
- `0105` temperatura do motor (coolant)
- `010F` temperatura do ar de admissão (IAT)
- `0110` MAF
- `010E` avanço da ignição
- `0111` throttle
- `010B` MAP
- `011F` runtime do motor
- `012F` nível de combustível
- `0142` tensão da bateria
- `015C` temperatura do óleo

Comandos AT adicionais:
- `ATRV` (retorna tensão em texto, ex: `13.80V`)

## 2) App Qt/QML (Python, sem build)

Pasta: `python-realdash-sender/`

### Requisitos
- Python 3.10+
- Dependências Python:
  - `PySide6`
  - `pyserial`

### Executar (Windows)
No terminal, dentro de `python-realdash-sender`:

```powershell
python -m pip install -r requirements.txt
python main.py
```

A interface abre direto via Python (não precisa compilar).

### Simulação Automática - Cenários Realistas
O app agora inclui um motor de simulação que reproduz conduções reais:

- **🏁 Marcha Lenta**: Motor em ponto morto (700 RPM)
- **🚗 Cidade**: Saídas, trânsito, paradas em semáforos, trocas de marcha, curvas moderadas (~60s)
- **🛣️  Rodovia**: Aceleração suave, cruise estável em alta RPM, variações de velocidade (~60s)
- **🏎️  Esportivo**: Lançamentos agressivos, alta carga de motor, curvas com frenagens, performance (~50s)

Cada cenário simula pressão real no motor: reações de throttle, MAP, MAF, variações de avanço, mudanças de temperatura conforme RPM/carga.

### Opcional: versão C++/CMake
Se quiser manter a versão compilada, ela está em `qt-realdash-sender/`.

## 3) Fluxo de uso

1. Faça upload do firmware no ESP32 (`PlatformIO`).
2. Abra o app Qt/QML.
3. Selecione a porta COM do ESP32 e conecte.
4. Ajuste os sliders/valores (envio automático por intervalo em ms).
5. No celular/tablet, conecte o RealDash ao Bluetooth `ESP32-ELM327`.
6. Configure no RealDash tipo OBD/ELM327 Bluetooth.

## 4) Observações

- A porta serial no app usa padrão **115200**.
- O envio é textual, simples e fácil de integrar com outros simuladores/fonte de dados.
- Para bateria no RealDash, o simulador responde tanto `0142` quanto `ATRV`.
- Se quiser, o próximo passo é adicionar leitura de um CSV ou replay de log para rodar cenários automaticamente.
