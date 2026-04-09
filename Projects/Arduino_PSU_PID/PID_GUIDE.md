# Explicação - Controle PID e Auto-Tuning

## O que é PID?

Um controlador PID (Proporcional-Integral-Derivativo) é um mecanismo de controle que ajusta uma saída (PWM) baseado no erro entre um setpoint (valor desejado) e o valor atual.

```
Erro = Setpoint - Saída_Atual
Saída_PID = Kp × Erro + Ki × ∫Erro + Kd × d(Erro)/dt
```

## Componentes do PID

### 1. Termo Proporcional (Kp)

- **O quê**: Proporcional ao erro atual
- **Efeito**: Quanto maior o erro, maior a correção
- **Problema**: Sozinho, deixa erro residual (offset)
- **Ajuste**: 
  - Aumentar → resposta mais rápida mas oscila
  - Diminuir → resposta lenta mas estável

**Valores típicos**: 1.0 - 5.0

### 2. Termo Integral (Ki)

- **O quê**: Soma histórica dos erros
- **Efeito**: Remove erro estacionário (offset)
- **Problema**: Pode causar overshoot (excesso)
- **Ajuste**:
  - Aumentar → remove offset mais rápido
  - Diminuir → evita overshoot

**Valores típicos**: 0.1 - 1.0

### 3. Termo Derivativo (Kd)

- **O quê**: Taxa de mudança do erro
- **Efeito**: Antecipa e amortece oscilações
- **Problema**: Sensível a ruído
- **Ajuste**:
  - Aumentar → mais amortecimento
  - Diminuir → menos influência ao derivativo

**Valores típicos**: 0.05 - 0.5

## Comportamento por Tipo de Tuning

### Subamortecido (Oscila)
```
Saída
  ↑
  │     ╱╲    ╱╲    ╱╲
  |    ╱  ╲  ╱  ╲  ╱  ╲
  |   ╱    ╲╱    ╲╱
  |──────────────────→ Tempo
```
**Solução**: Aumentar Kd ou reduzir Kp

### Criticamente Amortecido (Ideal)
```
Saída
  ↑
  │     ╱─────
  |    ╱
  |   ╱
  |──────────────→ Tempo
```
**Características**: Sem oscilação, sem muita demora

### Superamortecido (Lento)
```
Saída
  ↑
  │         ╱────
  |        ╱
  |      ╱
  |    ╱
  |──────────────→ Tempo
```
**Solução**: Aumentar Kp ou aumentar Ki

## Método de Ziegler-Nichols (Auto-Tuning)

Este código implementa uma versão simplificada baseada no método de Ziegler-Nichols:

### Passo 1: Encontrar Ganho Crítico (Ku)
- Defini Kp = Ku (ganho onde o sistema oscila continuamente)
- Observar amplitude e período das oscilações
- Medir: **Amplitude (A)** e **Período (Tu)**

### Passo 2: Calcular Ganhos
```
Kp = 0.6 × Ku
Ki = 1.2 × Ku / Tu
Kd = 0.075 × Ku × Tu
```

### Passo 3: Refinar (se necessário)
Ajustar manualmente se o sistema ainda não responde bem.

## Como Usar Auto-Tuning Neste Código

### Procedimento

1. **Ativar Auto-Tuning**
```
T  → Inicia processo
```

2. **O sistema fará**
   - Aplicar degrau de PWM (128)
   - Variar PWM em senóide para provocar oscilação
   - Medir oscilações da tensão de saída
   - Calcular Ku, Tu, A

3. **Ganhos serão calculados automaticamente**
   - Resposta recebida: `TUNE:COMPLETE KP=X.XX KI=X.XX KD=X.XX`
   - Novos ganhos já estarão ativos

4. **Refinar se necessário**
```
P2.5   → Ajustar Kp se oscila muito
D0.2   → Aumentar Kd para mais amortecimento
```

## Sintonização Manual

Se preferir não usar auto-tuning:

### Método 1: Tentativa e Erro

1. **Começar com Kd = 0, Ki = 0**
   ```
   D0
   I0
   P1.0
   ```

2. **Aumentar Kp até oscilação**
   ```
   P1.5
   P2.0
   P2.5  ← Oscila? OK
   ```

3. **Adicionar Kd para amortecimento**
   ```
   D0.1
   D0.15  ← Sem oscilação? OK
   ```

4. **Adicionar Ki para remover offset**
   ```
   I0.1
   I0.2
   I0.3  ← Sem offset? OK
   ```

### Método 2: Cohen-Coon
Se encontrou oscilação com Kp = Kc:

```
Período crítico = Tu
Amplitude = A

Kp = 1.33 × Kc
Ki = 0.66 × Kc / Tu
Kd = 0.11 × Kc × Tu
```

## Gráfico de Influência

```
┌─────────┬─────────┬─────────┐
│ Efeito  │ Aumentar│ Diminuir│
├─────────┼─────────┼─────────┤
│ Kp      │ Rápido  │ Lento   │
│         │ Oscila  │ Estável │
├─────────┼─────────┼─────────┤
│ Ki      │ Sem     │ Tem     │
│         │ Offset  │ Offset  │
│         │ Oscila  │ Estável │
├─────────┼─────────┼─────────┤
│ Kd      │ Suave   │ Oscila  │
│         │ Estável │ Rápido  │
└─────────┴─────────┴─────────┘
```

## Valores Recomendados para Fonte de Bancada

| Tipo de Carga | Kp | Ki | Kd |
|---|---|---|---|
| Resistiva (luz) | 3.0 | 0.5 | 0.1 |
| Bateria | 2.0 | 0.3 | 0.15 |
| Motor DC | 1.5 | 0.2 | 0.2 |
| Múltiplas cargas | 2.5 | 0.4 | 0.12 |

## Verificação de Estabilidade

Após sintonizar, verificar:

```
E1        → Ativar
V12       → Definir 12V
G         → Ler status por ~30 segundos
```

Observar:
- ✓ Sobe para 12V em <1 segundo
- ✓ Sem oscilação
- ✓ Mantém 12V estável
- ✓ Responde rápido a mudanças

## Limitações e Considerações

### Performance Limitada Por

1. **Frequência de Amostragem**: 100ms
   - Adequado para fonte de bancada
   - Ideal para cargas que mudam lentamente

2. **Resolução ADC**: 10 bits (0-1023)
   - Tensão: ~0.024V por LSB
   - Corrente: ~0.003A por LSB

3. **Saturação de PWM**: 0-255
   - Se erro muito grande, PWM fica em máximo/mínimo
   - Ganhos podem precisar ajuste

### Cases Especiais

**Se a carga varia dinamicamente** (ex: mudança súbita de corrente):
- Aumentar Kd para melhor resposta
- Aumentar Ki para compensar rápido

**Se há muito ruído** (oscilações de alta frequência):
- Aumentar tempo de amostragem a 200ms
- Aumentar Kd
- Filtrar entrada ADC com capacitor

## Recursos Adicionais

- Ziegler-Nichols Method: https://en.wikipedia.org/wiki/Ziegler%E2%80%93Nichols_method
- PID Control Tuning: https://en.wikipedia.org/wiki/PID_controller
- Klipper Auto-Tuning: https://www.klipper3d.org/
