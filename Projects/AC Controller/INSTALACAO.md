# Sistema Inteligente de Controle de AC - Ford Fiesta 2004 Rocam 1.6

## Descrição do Sistema
Este sistema desliga automaticamente o compressor do ar condicionado durante situações de alta carga e baixo RPM (subidas, arrancadas) para melhorar o desempenho do motor, religando-o automaticamente quando as condições voltam ao normal.

## Características
- ✅ Desliga AC em acelerações fortes com RPM baixo
- ✅ Desliga AC em subidas (alta carga MAP) com RPM baixo
- ✅ Desliga AC em marcha lenta muito baixa
- ✅ Religa AC automaticamente quando condições normalizam
- ✅ Não interfere nas trocas de marchas normais
- ✅ Delays configuráveis para evitar cortes desnecessários

## Hardware Necessário

### Componentes
1. **ATtiny85** (microcontrolador)
2. **Relé 12V** (para controlar o compressor do AC)
3. **Resistores** (para divisores de tensão nos sensores)
4. **Diodo de proteção** (1N4007 ou similar para o relé)
5. **Capacitores de filtro** (100nF nos sensores analógicos)
6. **Fonte de alimentação 5V** (regulador 7805 ou similar)

## Esquema de Ligação

### Pinagem do ATtiny85
```
        ATtiny85
     ┌────────────┐
RESET│1  ∪    8│VCC (5V)
 PB3 │2       7│PB2 (A1) - MAP Sensor
 PB4 │3 (A2)  6│PB1 - Relé AC
 GND │4       5│PB0
     └────────────┘
```

### Conexões Detalhadas

#### 1. Sensor MAP (PB2 / A1)
```
Sensor MAP → Divisor de tensão → PB2
                                  ├── 100nF → GND

Divisor de tensão (se MAP for 0-5V):
MAP Sensor ──┬─── 1kΩ ──┬─── PB2
             │          │
             └─ 1kΩ ─── GND
```

#### 2. Sensor TPS (PB4 / A3 / Pin 3)
```
TPS Signal → Divisor de tensão → PB4
                                  ├── 100nF → GND

TPS do carro ──┬─── 1kΩ ──┬─── PB4
               │          │
               └─ 1kΩ ─── GND
```

#### 3. Sinal do Injetor (PB3 / Pin 2)
```
Injetor #1 ──┬─── 10kΩ ──┬─── PB3
             │           │
             └─ 1kΩ ──── GND
(Dividir sinal 12V para 5V)
```

#### 4. Relé do Compressor AC (PB1 / Pin 6)
```
PB1 ──┬─── 1kΩ ──┬─── Base (Transistor 2N2222)
      │          │
      └── LED ───┴─── GND (indicador visual opcional)

Transistor:
Coletor ─── Bobina do Relé ─── +12V
Emissor ─── GND
Bobina do Relé ─── Diodo (1N4007) ─── +12V (proteção)

Contatos do Relé:
NA ─── Compressor AC
COM ─── +12V do carro
```

#### 5. Alimentação
```
+12V do carro ─── 7805 ─── +5V (ATtiny85)
                   │
                  GND
                   
Capacitores:
+12V ─── 100µF ─── GND (entrada 7805)
+5V  ─── 100µF ─── GND (saída 7805)
```

## Instalação no Veículo

### Localização dos Sinais

#### 1. Sensor MAP
- **Localização**: Tubo de admissão do motor
- **Sinal**: 0-5V (vácuo baixo = tensão alta)
- **Cor do fio**: Normalmente verde ou amarelo

#### 2. Sensor TPS
- **Localização**: Corpo de borboleta
- **Sinal**: 0-5V (borboleta fechada = ~0.5V, aberta = ~4.5V)
- **Fio de sinal**: Normalmente azul ou cinza

#### 3. Injetor
- **Localização**: Qualquer um dos 4 injetores
- **Sinal**: Pulsos 12V negativos
- **Use o fio de comando do injetor #1**

#### 4. Relé do Compressor
- **Localização**: Próximo ao compressor ou no compartimento do motor
- **Interromper**: Fio de comando da embreagem eletromagnética
- **Método**: Inserir o relé em série com o comando original

### Passos de Instalação

1. **Prepare a central**
   - Monte todos os componentes em uma caixa plástica
   - Use conectores automotivos para facilitar manutenção

2. **Conecte os sensores**
   - Identifique os fios do MAP e TPS
   - Use conectores "T" ou pinos de derivação
   - Não corte os fios originais

3. **Conecte o sinal do injetor**
   - Identifique o injetor #1
   - Faça uma derivação do sinal de comando
   - Use divisor de tensão para proteger o ATtiny85

4. **Instale o relé do AC**
   - Localize o relé/fio do compressor do AC
   - Insira o relé do sistema em série
   - Mantenha a ligação original como backup

5. **Alimentação**
   - Conecte ao +12V comutado (desliga com a chave)
   - Use fusível de 3A na alimentação
   - Aterramento no chassi

## Calibração e Ajustes

### Parâmetros Ajustáveis no Código

```cpp
// Thresholds de RPM
#define RPM_LOW_THRESHOLD   2000  // Ajustar para seu motor
#define RPM_HIGH_THRESHOLD  4500  // Ajustar para seu motor

// Thresholds de Carga
#define TPS_HIGH_THRESHOLD  70    // 70% de abertura da borboleta
#define MAP_HIGH_THRESHOLD  75    // 75% de carga do motor

// Delays
#define AC_CUTOFF_DELAY     200   // Tempo antes de cortar (ms)
#define AC_RESTORE_DELAY    1000  // Tempo antes de religar (ms)
```

### Procedimento de Calibração

1. **Teste em marcha lenta**
   - Motor deve manter RPM estável
   - AC deve permanecer ligado

2. **Teste em aceleração suave**
   - AC deve permanecer ligado
   - Sem cortes desnecessários

3. **Teste em aceleração forte (kickdown)**
   - AC deve desligar após ~200ms
   - AC deve religar após normalizar

4. **Teste em subida**
   - AC deve desligar em subidas íngremes com carga alta
   - AC deve religar ao completar a subida

## Segurança

⚠️ **IMPORTANTE**:
- Teste o sistema antes de uso permanente
- Mantenha sempre a opção de desligar manualmente o AC
- Não modifique o sistema elétrico do carro sem conhecimento
- Use fusíveis adequados em todas as ligações
- Evite curto-circuitos verificando todas as conexões

## Troubleshooting

### AC não desliga nunca
- Verifique se os sensores estão conectados corretamente
- Verifique se o sinal do injetor está chegando
- Ajuste os thresholds para valores mais baixos

### AC desliga muito frequentemente
- Aumente `AC_CUTOFF_DELAY`
- Aumente `RPM_LOW_THRESHOLD`
- Reduza `TPS_HIGH_THRESHOLD` e `MAP_HIGH_THRESHOLD`

### AC não religa
- Verifique `AC_RESTORE_DELAY`
- Verifique se o relé está funcionando
- Verifique a alimentação do sistema

### RPM incorreto
- Verifique o número de cilindros no código
- Verifique o sinal do injetor
- Ajuste o cálculo de RPM

## Melhorias Futuras

- [ ] Adicionar display para debug
- [ ] Adicionar log de eventos
- [ ] Adicionar sensor de temperatura do motor
- [ ] Adicionar botão de override manual
- [ ] Adicionar comunicação serial para diagnóstico

## Suporte

Para dúvidas ou problemas, ajuste os parâmetros no código conforme necessário para seu veículo específico.
