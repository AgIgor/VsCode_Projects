# Guia de Uso - Fonte de Bancada PID

## Conexão Serial

Conecte o Arduino via USB. Use um monitor serial com:
- **Baud Rate**: 115200 bps
- **Data bits**: 8
- **Stop bits**: 1
- **Parity**: None

## Comandos Disponíveis

### Operações Básicas

| Comando | Exemplo | Descrição |
|---------|---------|-----------|
| `V<valor>` | `V12.5` | Define tensão alvo (0-24V) |
| `C<valor>` | `C0.5` | Define corrente alvo (0-3A) |
| `E<0\|1>` | `E1` | Ativa (1) ou desativa (0) a saída |
| `G` | `G` | Lê status atual |
| `H` | `H` | Exibe ajuda |

### Sintonização PID Manual

| Comando | Exemplo | Descrição |
|---------|---------|-----------|
| `P<valor>` | `P2.5` | Define ganho proporcional (Kp) |
| `I<valor>` | `I0.3` | Define ganho integral (Ki) |
| `D<valor>` | `D0.15` | Define ganho derivativo (Kd) |

### Auto-Tuning

| Comando | Descrição |
|---------|-----------|
| `T` | Inicia auto-tuning estilo Klipper |

## Exemplos de Uso

### 1. Ligar e definir 12V, 0.5A

```
E1         → Ativa a saída
V12.0      → Define 12V
C0.5       → Define 0.5A
G          → Lê status
```

### 2. Auto-tuning Automático

```
T          → Inicia auto-tuning
           → Aguarda ~10 segundos
           → Recebe: TUNE:COMPLETE KP=... KI=... KD=...
```

Após o tuning, os ganhos são automaticamente ajustados para resposta ótima.

### 3. Ajuste Manual do PID

```
P2.0       → Kp = 2.0 (controle proporcional)
I0.5       → Ki = 0.5 (remove erro estacionário)
D0.1       → Kd = 0.1 (amortecimento)
```

## Monitoramento em Tempo Real

O sistema envia a cada 500ms:

```
>V:12.45 I:0.32 P:180
>V:12.47 I:0.31 P:179
...
```

Onde:
- `V`: Tensão de saída atual (V)
- `I`: Corrente de saída atual (A)
- `P`: Valor PWM (0-255)

## Interpretação de Respostas

| Resposta | Significado |
|----------|---|
| `SV:12.50` | Tensão alvo confirmada em 12.50V |
| `SC:0.50` | Corrente alvo confirmada em 0.50A |
| `EN:ON` | Saída ativada |
| `EN:OFF` | Saída desativada |
| `KP:2.0500` | Kp atualizado para 2.05 |
| `TUNE:START` | Auto-tuning iniciado |
| `TUNE:COMPLETE KP=2.15 KI=0.45 KD=0.08` | Auto-tuning concluído com novos ganhos |

## Troubleshooting

### Tensão não sobe acima de X volts

1. Verificar se a fonte de entrada tem suficiente potência
2. Conferir calibração do divisor resistivo (comando `G`)
3. Aumentar `Kp` para maior resposta
4. Verificar se o MOSFET está bem conectado

### Tensão oscila muito

1. Reduzir `Kp` (menos ganho proporcional)
2. Aumentar `Kd` (mais amortecimento)
3. Executar auto-tuning com `T`

### Tensão sobe lentamente

1. Aumentar `Kp`
2. Aumentar `Ki` para remover erro residual
3. Usar auto-tuning

### Corrente não é lida corretamente

1. Verificar calibração do shunt
2. Confirmar ganho do amplificador (padrão: 10x)
3. Verificar se há amplificador de corrente ou não

## Script Python para Controle (Opcional)

```python
import serial
import time

ser = serial.Serial('COM3', 115200)  # Ajuste a porta

# Aguardar inicialização
time.sleep(2)

# Ligar e definir 12V
ser.write(b'E1\n')
ser.write(b'V12.0\n')

# Ler status continuamente
for _ in range(60):
    ser.write(b'G\n')
    response = ser.readline().decode()
    print(response)
    time.sleep(1)

ser.close()
```

## Limitações

- Máximo 24V de saída
- Máximo 3A de corrente
- PWM @ ~490 Hz (D3 no Arduino UNO)
- Precisão de tensão: ~0.1V
- Precisão de corrente: ±10mA

## Pinagem Resumo

```
D3  → Controle MOSFET
A0  → Leitura tensão
A1  → Leitura corrente
GND → Referência
```
