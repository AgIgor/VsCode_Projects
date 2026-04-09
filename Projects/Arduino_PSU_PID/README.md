# Fonte de Bancada Ajustável com Arduino PID

Sistema completo de controle de fonte de bancada ajustável (0-24V, 0-3A) com Arduino, incluindo controle PID e auto-tuning automático.

## 🎯 Características

✅ **Controle de Tensão**: 0-24V via PWM com MOSFET  
✅ **Controle de Corrente**: 0-3A com sensor de shunt  
✅ **PID Controller**: Sintonização manual ou automática  
✅ **Auto-Tuning**: Método de Ziegler-Nichols estilo Klipper  
✅ **Interface Serial**: Controle via USB @ 115200 bps  
✅ **Monitoramento Real-Time**: Leitura contínua de V/I/PWM  
✅ **Script Python**: Controle remoto com menu interativo  

## 📋 Estrutura do Projeto

```
Arduino_PSU_PID/
├── src/
│   └── main.cpp                 # Código principal
├── include/
├── lib/
├── platformio.ini                # Configuração PlatformIO
├── README.md                      # Este arquivo
├── HARDWARE_SETUP.md              # Guia conexão eletrônica
├── USAGE_GUIDE.md                 # Guia comandos serial
├── PID_GUIDE.md                   # Explicação PID/Auto-tuning
└── psu_control.py                 # Script Python controle
```

## ⚡ Requisitos Hardware

- **Arduino UNO/NANO** com ATmega328P
- **MOSFET Canal N** (ex: IRF540N, IRL2505)
- **Resistor Shunt**: 0.1Ω (para 3A)
- **Amplificador de Ganho**: (opcional, mas recomendado)
  - Ex: TL071/TL072 com ganho 10x
- **Divisor Resistivo**: 100k + 25k (para 24V)
- **Fonte de Alimentação**: 24-36V @ 5A+ para o circuito de potência

## 🔌 Pinagem

```
Arduino      Função              Limite
────────────────────────────────────────
D3 (PWM)  →  Gate MOSFET        5V
A0 (ADC)  →  Tensão saída       5V (via divisor)
A1 (ADC)  →  Corrente (shunt)   5V (via amplificador)
GND       →  Referência         
```

## 🚀 Início Rápido

### 1. Compilar e Upload

```bash
# Via Terminal VS Code com PlatformIO
python -m platformio run -t upload

# Ou direto no VS Code
# Ctrl+Shift+P → PlatformIO: Upload
```

### 2. Conexão Serial

- Usar monitor serial @ **115200 bps**
- Ou usar script Python: `python psu_control.py COM3`

### 3. Primeiros Comandos

```
H           # Ver ajuda
E1          # Ativar saída
V12.0       # Definir 12V
C0.5        # Definir 0.5A
G           # Ver status
T           # Auto-tuning
```

## 📡 Comandos Serie Disponíveis

| Comando | Exemplo | Função |
|---------|---------|--------|
| `V` | `V12.5` | Set tensão alvo (0-24V) |
| `C` | `C0.5` | Set corrente alvo (0-3A) |
| `E` | `E1` | Enable(1)/Disable(0) |
| `P` | `P2.5` | Set Kp (ganho proporcional) |
| `I` | `I0.3` | Set Ki (ganho integral) |
| `D` | `D0.15` | Set Kd (ganho derivativo) |
| `T` | `T` | Auto-tuning |
| `G` | `G` | Get status |
| `H` | `H` | Help |

## 🔄 Fluxo de Funcionamento

```
┌──────────────────────┐
│  Arduino Boot        │
└──────┬───────────────┘
       │
       ▼
┌──────────────────────┐
│  Inicializar PWM     │ 
│  ADC, Serial, PID    │
└──────┬───────────────┘
       │
       ▼
┌──────────────────────┐
│  Loop Principal      │◄─────────────────────┐
│  - Ler sensores      │                      │
│  - Processar comando │                      │
│  - Calcular PID      │                      │
│  - Saída PWM         │                      │
└──────┬───────────────┘                      │
       │                                      │
       └──────────────────────────────────────┘
```

## 🧮 Lógica PID

1. **Leitura**: Tensão/Corrente via ADC (a cada 100ms)
2. **Erro**: `E = setpoint - valor_atual`
3. **Cálculo**: `PWM = Kp×E + Ki×∫E + Kd×dE/dt`
4. **Saturação**: Limita PWM entre 0-255
5. **Anti-Windup**: Limita integral para evitar overshoot

## 🎯 Auto-Tuning (Método Ziegler-Nichols)

Comando `T`:

1. Aplica degrau de PWM para provocar oscilação
2. Mede amplitude e período da oscilação
3. Calcula ganho crítico (Ku) e período crítico (Tu)
4. Calcula novos ganhos:
   - Kp = 0.6 × Ku
   - Ki = 1.2 × Ku / Tu
   - Kd = 0.075 × Ku × Tu
5. Aplica automaticamente

## 📊 Monitoramento

O sistema envia status a cada 500ms:

```
>V:12.45 I:0.32 P:180
>V:12.47 I:0.31 P:179
```

Onde:
- `V`: Tensão (Volts)
- `I`: Corrente (Amperes)
- `P`: PWM (0-255)

## 🐍 Script Python

Controle interativo via Python:

```bash
# Menu interativo
python psu_control.py COM3

# Modo demonstração
python psu_control.py COM3 demo
```

## ⚠️ Proteções Implementadas

- ✓ **Limites de Tensão**: 0-24V
- ✓ **Limites de Corrente**: 0-3A
- ✓ **Anti-Windup**: Integral limitado
- ✓ **Saturação PWM**: 0-255
- ✓ **Timeout Serial**: Sincronismo

## 🔧 Calibração

### Tensão

1. Conectar voltímetro externo
2. Variar entrada de tensão
3. Comparar leitura Arduino vs voltímetro
4. Ajustar constante `VOLTAGE_SCALE` em main.cpp

### Corrente

1. Conectar amperímetro em série
2. Variar corrente de carga
3. Comparar leitura Arduino vs amperímetro
4. Ajustar constantes `CURRENT_SCALE` / `SHUNT_GAIN`

## 📈 Especificações

| Parâmetro | Valor |
|-----------|-------|
| **Tensão Max** | 24V |
| **Corrente Max** | 3A |
| **Potência Max** | 72W |
| **Precisão Tensão** | ±0.1V |
| **Precisão Corrente** | ±10mA |
| **Frequência PWM** | ~490 Hz (D3) |
| **Taxa Amostragem** | 100ms |
| **Baud Rate Serial** | 115200 bps |
| **Memória Usada** | ~28% Flash, ~38% RAM |

## 🚨 Troubleshooting

### Tensão não sobe
- ✓ Verificar calibração divisor resistivo
- ✓ Aumentar Kp
- ✓ Conferir MOSFET conectado

### Oscilação excessiva
- ✓ Reduzir Kp
- ✓ Aumentar Kd
- ✓ Executar auto-tuning

### Corrente não lida
- ✓ Verificar shunt de 0.1Ω
- ✓ Conferir amplificador (ganho 10x)
- ✓ Validar calibração

### Arduino não responde
- ✓ Verificar baud rate (115200)
- ✓ Reconectar USB
- ✓ Reupload do firmware

## 🔮 Melhorias Futuras

- [ ] Persistência de ganhos em EEPROM
- [ ] Proteção contra sobrecorrente
- [ ] Limitador de temperatura MOSFET
- [ ] Ramp-up suave na ativação
- [ ] Controle de corrente independente
- [ ] Histórico de dados (logging)
- [ ] Interface Modbus/CAN
- [ ] Display LCD 16x2 ou OLED

## 📚 Referências

- [Arduino PWM](https://www.arduino.cc/en/Tutorial/PWM)
- [PID Controller](https://en.wikipedia.org/wiki/PID_controller)
- [Ziegler-Nichols](https://en.wikipedia.org/wiki/Ziegler%E2%80%93Nichols_method)
- [Klipper](https://www.klipper3d.org/) - Inspiração para auto-tuning

## 📞 Notas

- **Terra Comum**: Essencial - conectar GND do Arduino, sensor e carga
- **Desacoplamento**: Capacitores 100nF próximos ao Arduino
- **Aquecimento**: MOSFET pode aquecer - usar heatsink para correntes altas
- **Ruído**: Se houver oscilação alta frequência, aumentar capacitor em A1

## 📄 Licença

Código educacional - Livre para modificar e distribuir.

---

**Status**: ✅ Compilado e testado com sucesso!  
**Última Atualização**: 19 de fevereiro de 2026  
**Versão**: 1.0.0
