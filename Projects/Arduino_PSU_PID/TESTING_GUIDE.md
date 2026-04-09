# Teste e Diagnóstico - Fonte PID

Guia para testar e validar o funcionamento do sistema.

## ✅ Checklist Pré-Operacional

### Hardware
- [ ] Resistência do shunt é 0.1Ω?
- [ ] Divisor resistivo: 100k + 25k?
- [ ] MOSFET está bem conectado?
- [ ] Capacitores de desacoplamento (100nF) instalados?
- [ ] Todas as conexões soldadas corretamente?

### Arduino
- [ ] USB conectado?
- [ ] LED "L" piscando (firmware carregado)?
- [ ] Monitor serial abre sem erros?

### Software
- [ ] Compilação com sucesso (28% Flash, 38% RAM)?
- [ ] Serial monitor @ 115200 bps?
- [ ] Comando 'H' exibe ajuda?

## 🔍 Teste 1: Comunicação Serial

**Objetivo**: Verificar se Arduino responde a comandos

```
Enviar: H
Esperado: Menu com lista de comandos

Enviar: G
Esperado: V:0.00 I:0.00 P:0 (ou similares)
```

**Se não funcionar**:
- Verificar baud rate (deve ser exatamente 115200)
- Desconectar/reconectar USB
- Verificar LED "D13" sinalizador

---

## 🔌 Teste 2: Leitura de Tensão

**Equipamento**: Voltímetro externo

```
1. Desativar saída (E0)
2. Sem tensão externa aplicada:
   Comando: G
   Esperado: V:0.00

3. Aplicar 5V na entrada:
   Comando: G
   Esperado: V: ~5.00 (±0.2V)

4. Aplicar 12V:
   Esperado: V: ~12.00 (±0.2V)

5. Aplicar 24V:
   Esperado: V: ~24.00 (±0.2V)
```

**Fórmula de calibração**:
```
Se leitura ≠ tensão real
Fator = Tensão_Real / Tensão_Lida
Novo VOLTAGE_SCALE = VOLTAGE_SCALE * Fator
```

**Exemplo**:
```
Aplica 12V → Arduino lê 11.5V
Fator = 12.0 / 11.5 = 1.043
Novo VOLTAGE_SCALE = (24.0 / 1023.0) * 1.043 = 0.0245
```

---

## ⚡ Teste 3: Leitura de Corrente

**Equipamento**: Amperímetro, fonte variável

```
1. Desativar saída (E0)

2. Sem passagem de corrente:
   Comando: G
   Esperado: I:0.00

3. Aplicar 0.5A:
   Comando: G
   Esperado: I: ~0.50 (±0.05A)

4. Aplicar 1.0A:
   Esperado: I: ~1.00 (±0.05A)

5. Aplicar 3.0A (máximo):
   Esperado: I: ~3.00 (±0.10A)
```

**Fórmula de calibração (similar à tensão)**:
```
Se leitura ≠ corrente real
Fator = Corrente_Real / Corrente_Lida
Novo CURRENT_SCALE = CURRENT_SCALE * Fator
```

---

## 🎛️ Teste 4: Controle PWM

**Objetivo**: Verificar se PWM varia a tensão de saída

```bash
# Terminal serial:
E1          # Ativar
V0.0        # 0V (PWM deve ir para 0)
G           # Verificar P:0

V5.0        # 5V (PWM deve aumentar)
G           # Verificar P: ~50 (depende da entrada)

V12.0       # 12V (PWM médio)
G           # Verificar P: ~128

V24.0       # 24V (PWM máximo)
G           # Verificar P: ~255
```

**Diagnosticar se PWM não funciona**:
- [ ] MOSFET recebe tensão gate?
- [ ] MOSFET tem Drain e Source invertidos?
- [ ] Gate resistor 1k presente?
- [ ] Capacitor desacoplamento próximo ao Arduino?

---

## 🎯 Teste 5: Controle PID Básico

**Objetivo**: Testar resposta PID com carga resistiva

```bash
# Terminal:
E1              # Ativar

V5.0            # Definir 5V
# Medir tempo de subida até 5V (deve ser <1s)
# Observar se oscila

V10.0           # Aumentar para 10V
# Medir tempo (deve ser rápido)

V12.0           # Voltar para estável
# Observar estabilidade
```

**Esperado**:
- ✓ Sobe em ~300-500ms
- ✓ Sem overshoot ou mínimo
- ✓ Mantém estável ±0.1V

**Se oscilar muito**:
```bash
D0.2            # Aumentar Kd
D0.3            # Mais amortecimento
```

**Se for muito lento**:
```bash
P3.0            # Aumentar Kp
I0.4            # Aumentar Ki
```

---

## 🤖 Teste 6: Auto-Tuning

**Objetivo**: Validar auto-tuning automático

```bash
# Terminal:
T               # Iniciar auto-tuning

# Aguardar ~20-30 segundos
# Deve mostrar: TUNE:COMPLETE KP=X.XX KI=X.XX KD=X.XX

# Testar nova resposta:
E1
V12.0
# Deve responder melhor que antes
```

**Interpretar resultado**:
```
TUNE:COMPLETE KP=2.15 KI=0.45 KD=0.08

Baseado em:
- Amplitude de oscilação
- Período de oscilação
- Ganho crítico calculado
```

---

## 📊 Teste 7: Estabilidade em Carga

**Objetivo**: Testar resposta com carga dinâmica

```bash
# Preparar:
# - Resistor variável (reostato) como carga
# - Monitor serial aberto

E1              # Ativar
V12.0           # Set 12V
C1.5            # Set 1.5A

# Variar resistência lentamente
# Observar V e I em tempo real

# Esperado:
# > Quando aumenta corrente (diminui resistência)
#   Tensão deve se manter ~12V
#   Corrente deve aumentar proporcionalmente

# > Quando diminui corrente (aumenta resistência)
#   Tensão deve se manter ~12V
```

---

## 🔬 Teste 8: Resposta em Degrau

**Objetivo**: Testar resposta a mudança súbita

```bash
# Preparação: Carga resistiva constante

E1
V5.0
# Aguardar estabilização (5-10 segundos)

# Degrau: 5V → 15V
V15.0
# Medir: Tempo subida, Overshoot, Oscilação

# Se houver problema:
# - Mudar Kp, Ki, Kd
# - Ou executar novo auto-tuning
```

**Gráfico esperado**:
```
Tensão
  |      ╱─────
  |     ╱
  |    ╱
  |───────────→ Tempo
  
Suba suave (sem oscilação)
Sem overshoot excessivo
```

---

## 🧪 Teste 9: Log de Dados

Capturar dados para análise:

```bash
# Python script para capturar dados:

import serial
import csv
from datetime import datetime

ser = serial.Serial('COM3', 115200)
ser.write(b'E1\nV12.0\n')

with open('log.csv', 'w') as f:
    writer = csv.writer(f)
    writer.writerow(['Time', 'V', 'I', 'PWM'])
    
    start = datetime.now()
    for _ in range(300):  # 300 * 0.5s = 150s
        ser.write(b'G\n')
        line = ser.readline().decode()
        # Fazer parse e salvar
        elapsed = (datetime.now() - start).total_seconds()
        # writer.writerow([elapsed, ...])

ser.close()
```

Depois analisar em Excel ou Python.

---

## 🚨 Teste de Segurança

**Limites**:
```bash
# Testar limites máximos
V25.0       # Deve clampar em 24V
C4.0        # Deve clampar em 3A
P10.0       # Deve ignorar valor inválido

# Testar desativação
E1
V12.0
E0          # Deve desativar - V deve cair para 0
G           # V:0.00 P:0
```

---

## 📋 Matriz de Testes

| Teste | OK? | Valor Medido | Ação Corretiva |
|-------|-----|--------------|---|
| Serial responde | [ ] | | |
| Tensão 5V lida | [ ] | | Calibrar |
| Tensão 12V lida | [ ] | | Calibrar |
| Tensão 24V lida | [ ] | | Calibrar |
| Corrente 0.5A | [ ] | | Calibrar |
| Corrente 1.0A | [ ] | | Calibrar |
| PWM varia | [ ] | | Verificar MOSFET |
| PID sobe em <1s | [ ] | | Ajustar Kp |
| Sem oscilação | [ ] | | Ajustar Kd |
| Auto-tuning funciona | [ ] | | Verificar algoritmo |
| Carga dinâmica OK | [ ] | | Ajustar Ki |
| Limites respeitados | [ ] | | Verificar software |

---

## 🔧 Dicas de Diagnóstico

### MOSFET não ativa
```
Sintoma: PWM=255 mas tensão não sobe
Causa: Gate não aciona MOSFET
Solução:
  1. Medir tensão no gate (D3) com multímetro
  2. Deve variar de 0-5V conforme PWM
  3. Se constante, Arduino pode estar com problema
  4. Se varia, MOSFET pode estar defeituoso
```

### Oscilação alta frequência
```
Sintoma: Oscilação de ~1kHz
Causa: Falta de capacitor desacoplamento
Solução:
  1. Adicionar 100nF próximo ao Arduino
  2. Adicionar capacitor "bulk" 10µF
  3. Aumentar Kd para amortecimento
```

### ADC ruidoso
```
Sintoma: Leitura varia ±0.5V continuamente
Causa: Ruído na entrada ADC
Solução:
  1. Adicionar capacitor 100nF em A0 e A1
  2. Aumentar tempo de amostragem (200ms)
  3. Filtrar leitura (média móvel)
```

---

## ✅ Validação Final

Quando todos os testes passarem:

```bash
# Teste final integrado:
T                   # Auto-tuning
# Aguardar conclusão

E1                  # Ativar
V12.0               # 12V
G                   # Status OK?

# Resultado esperado:
# V: ~12.00
# I: ~0.00 (sem carga) ou esperado
# P: 128-180 (depende da entrada)
# Resposta suave sem oscilação

E0                  # Desativar
# Resultado: P:0, V:0.00
```

✅ Sistema pronto para uso!
