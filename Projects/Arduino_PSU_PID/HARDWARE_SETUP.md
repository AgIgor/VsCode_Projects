# Fonte de Bancada Ajustável com Arduino - Configuração de Hardware

## Pinagem Arduino UNO/NANO

```
PWM_PIN (D3)      → Gate do MOSFET (canal N)
VOLTAGE_PIN (A0)  → Entrada do divisor resistivo de tensão
CURRENT_PIN (A1)  → Entrada do sensor de corrente (shunt)
GND               → Referência de terra comum
```

## Circuito Divisor de Tensão (0-24V → 0-5V)

Para medir até 24V no pino A0 (máximo 5V):

```
Vin (24V)
   |
   R1 (100k)
   |
   +---[A0]---+
   |          |
   R2 (25k)   |
   |          |
   GND--------+

Fator de divisão: 25/(100+25) = 0.2
Vout = 24V * 0.2 = 4.8V máximo
```

**Calibração**: Ajuste `VOLTAGE_SCALE` no código conforme seu divisor:
- Se usar 100k + 25k: `VOLTAGE_SCALE = (24.0 / 1023.0)` ✓
- Para outros resistores: `VOLTAGE_SCALE = 24.0 / (5.0 / 1023.0 * (R1+R2) / R2)`

## Sensor de Corrente (Resistor Shunt)

Use um resistor shunt de **0.1Ω** em série com a carga (máximo 3A):

```
Carga (+)
   |
[Shunt 0.1Ω] → Cria queda de 0.3V em 3A
   |
   └─[INA219 ou amplificador com ganho 10]─→ [A1]
```

**Importante**: Para melhor precisão, use um amplificador de instrumentação (ex: TL071/072 com ganho 10).

Sem amplificador: Queda máxima = 0.3V (baixa resolução)
Com ganho 10: Queda no ADC = 3V (melhor resolução)

**Configuração**: 
- `SHUNT_RESISTOR = 0.1`
- `SHUNT_GAIN = 10.0` (ajuste conforme seu amplificador)

## Controle MOSFET Canal N

```
             Vcc
              |
              R(pull-down 10k)
              |
              D (Drain)
              |
         [MOSFET N]
              |
              S (Source) → Saída da fonte regulada
              |
            Carga
              |
             GND

Gate ← [1k] ← [D3 Arduino]
```

**Especificações recomendadas do MOSFET**:
- Vgs(th): 2-5V
- Rds(on) < 10mΩ @ Vgs=10V
- Dissipação: > 10W (com heatsink)
- Exemplos: IRF540N, IRL2505, IRLB3034PBF

## Montagem Geral

```
        Arduino
     ┌──────────┐
     │  D3(PWM) ├─────[1k]────→ MOSFET Gate
     │   A0     ├───→ Divisor de tensão
     │   A1     ├───→ Sensor de corrente
     │  GND     ├──→ Referência (terra comum)
     └──────────┘
         |
         └─[Fonte 5V regrada]

Estágio de Potência:
    Vcc(input 24-36V)
         |
      [MOSFET N]←─ Controlado por Arduino
         |
        [Carga] ← Divisor R + Shunt A1
         |
        GND
```

## Calibração dos Sensores

### Passo 1: Calibração de Tensão
1. Conectar voltímetro na saída da fonte
2. Usar um resistor variável (reostato) na entrada
3. Medir ADC vs tensão real
4. Ajustar `VOLTAGE_SCALE` se necessário

### Passo 2: Calibração de Corrente
1. Conectar amperímetro em série
2. Variar a corrente
3. Comparar leitura Arduino vs amperímetro
4. Ajustar `CURRENT_SCALE` se necessário

## Alimentação

- **Arduino**: Fonte regulada 5V @ 100mA
- **Amplificador de ganho**: Fonte dual ±15V ou única 5V
- **MOSFET**: Limpar capacitâncias (100nF próximo aos pinos)

## Proteções Recomendadas

```
Vcc ---[Fusível 10A]---→ [Diodo Schottky] ---→ Carga
                           (proteção reversa)

Entre D-S do MOSFET:
[Diodo rápido 1N4148] para proteção em curto
```

## Notas Importantes

1. **Terra comum**: Conectar GND do Arduino, sensor e carga
2. **Ruído**: Usar capacitores de desacoplamento 100nF perto do Arduino
3. **Aquecimento**: O MOSFET pode aquecer com correntes altas - usar heatsink
4. **Segurança**: Sempre desligar antes de reconectar circuitos
