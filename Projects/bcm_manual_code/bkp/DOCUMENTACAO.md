# Sistema de Controle de Botões e Temporizadores - Documentação Completa

## 📋 Recursos Disponíveis

### 1. **Detecção de Estados do Botão (Button Class)**

Detecta mudanças de estado com debounce e suporte a diferentes tipos de borda (rising/falling).

#### Características:
- Debouncing automático (20ms padrão)
- Detecção de borda de subida (RISING) ou descida (FALLING)
- Leitura estável do estado atual
- Configurável após criação

#### Uso Básico:
```cpp
// Criar botão com borda de descida
Button button(2, HIGH, 20, FALLING);

// No loop
button.update();

// Verificar se borda detectada
if (button.edgeDetected()) {
    Serial.println("Borda detectada!");
}

// Obter estado atual
uint8_t state = button.getState();  // HIGH ou LOW

// Mudar tipo de borda em tempo de execução
button.setEdgeType(RISING);
```

**Parâmetros do Construtor:**
- `buttonPin`: Pino do Arduino (2-13)
- `initialState`: Estado inicial (HIGH ou LOW)
- `debounceMs`: Tempo de debounce em milisegundos
- `edge`: Tipo de borda (RISING ou FALLING)

---

### 2. **Temporização de Saídas (OutputTimer Class)**

Sistema escalável para controlar saídas com delay de ligação e desligação.

#### Características:
- Delay de ligação (On-Delay): tempo até ligar após acionamento
- Delay de desligação (Off-Delay): tempo até desligar após acionamento
- delayOn = 0: liga instantâneamente
- delayOff = 0: desliga apenas ao soltar o botão
- Opção de iniciar delay off apenas ao soltar (delayOffOnRelease)
- Controle escalável (múltiplas saídas)

#### Uso Básico:
```cpp
// Saída com delay on=1s, delay off=2s
OutputTimer output(6, 1000, 2000);

// No loop
output.trigger(botaoPrecionado);  // true=pressionado, false=solto
output.update();

// Obter estado da saída
uint8_t outState = output.getOutputState();  // HIGH ou LOW

// Mudar delays em tempo de execução
output.setDelays(500, 1500);

// Verificar status do temporizzador
bool isOn = output.isTimingOn();
bool isOff = output.isTimingOff();
```

#### Exemplos Práticos:

**Exemplo 1: Liga instantâneo, desliga em 3s**
```cpp
OutputTimer output1(6, 0, 3000);  // delayOn=0, delayOff=3000
```

**Exemplo 2: Desliga apenas ao soltar o botão**
```cpp
OutputTimer output2(7, 500, 0);  // Liga em 0.5s, desliga ao soltar
```

**Exemplo 3: Delay off inicia apenas ao soltar**
```cpp
OutputTimer output3(8, 1000, 2000, true);  // delayOffOnRelease=true
```

---

### 3. **Lógica AND/OR (LogicGate Class)**

Sistema para combinar múltiplas entradas com lógica booleana.

#### Características:
- Suporta AND (todos devem ser verdadeiros)
- Suporta OR (pelo menos um deve ser verdadeiro)
- Escalável (qualquer número de entradas)
- Mude o modo em tempo de execução

#### Uso Básico:
```cpp
// Criar portão AND com 2 entradas
LogicGate gate1(2, AND_MODE);

// Criar portão OR com 3 entradas
LogicGate gate2(3, OR_MODE);

// Atualizar valores das entradas
gate1.setInput(0, button1.getState() == LOW);
gate1.setInput(1, button2.getState() == LOW);

// Avaliar lógica
if (gate1.evaluate()) {  // Retorna true se AMBOS LOW
    digitalWrite(OUTPUT_PIN, HIGH);
}

// Mudar modo em tempo de execução
gate1.setMode(OR_MODE);

// Verificar modo atual
LogicMode mode = gate1.getMode();
```

#### Exemplos Práticos:

**Exemplo 1: Saída ativada apenas se TODOS os botões pressionados (AND)**
```cpp
LogicGate systemLogic(4, AND_MODE);

// Todos devem estar LOW (pressionados)
systemLogic.setInput(0, button1.getState() == LOW);
systemLogic.setInput(1, button2.getState() == LOW);
systemLogic.setInput(2, button3.getState() == LOW);
systemLogic.setInput(3, button4.getState() == LOW);

if (systemLogic.evaluate()) {
    digitalWrite(SAIDA_SEGURANCA, HIGH);
}
```

**Exemplo 2: Saída ativada se QUALQUER botão pressionado (OR)**
```cpp
LogicGate alarmLogic(3, OR_MODE);

// Pelo menos um deve estar LOW
alarmLogic.setInput(0, sensor1 == LOW);
alarmLogic.setInput(1, sensor2 == LOW);
alarmLogic.setInput(2, sensor3 == LOW);

if (alarmLogic.evaluate()) {
    digitalWrite(ALARME, HIGH);
}
```

---

### 4. **Saída com Prioridade (PriorityOutput Class)**

Controle de uma saída por duas entradas com prioridade configurável.

#### Características:
- Uma entrada tem prioridade sobre a outra
- Escalável (múltiplas saídas com prioridades)
- Suporta delays de ligação e desligação
- Mude prioridade em tempo de execução

#### Uso Básico:
```cpp
// Input 0 (primary) tem prioridade sobre Input 1 (secondary)
PriorityOutput output(9, 0, 1, 1);  // priorityLevel=1

// Atualizar com estados dos botões
output.update(button1.getState() == LOW, button2.getState() == LOW);

// Obter estado da saída
uint8_t outState = output.getOutputState();

// Configurar delays
output.setDelays(500, 1000);  // delayOn=500ms, delayOff=1000ms

// Alterar prioridade em tempo de execução
output.setInputs(1, 0, 0);  // Input 1 agora tem prioridade
```

#### Exemplos Práticos:

**Exemplo 1: Parar de emergência tem prioridade**
```cpp
// Botão de parada (emergência) tem prioridade
PriorityOutput saida(9, 1, 0, 1);  // Input 1 (parada) = prioridade

// No loop
saida.update(
    button1.getState() == LOW,  // Comando normal
    button2.getState() == LOW   // Parada de emergência (prioridade)
);
```

**Exemplo 2: Sistema com dois controladores**
```cpp
// Controlador principal tem prioridade sobre remoto
PriorityOutput controlador(10, 0, 1, 1);

controlador.update(
    botaoPrincipal.getState() == LOW,
    botaoRemoto.getState() == LOW
);
```

---

### 5. **Timer Escalável (ScalableTimer Class)**

Sistema de contagem regressiva com recursos avançados.

#### Características:
- Contador de tempo configurável
- Callbacks ao completar (funções assincronas)
- Monitoramento de progresso (0-100%)
- Tempo restante em ms
- Múltiplos timers independentes

#### Uso Básico:
```cpp
// Criar timer de 5 segundos
ScalableTimer timer(5000);

// Callback ao completar
void onComplete() {
    digitalWrite(LED, HIGH);
}

timer.onTimerComplete(onComplete);

// No setup
timer.start();

// No loop
timer.update();

// Verificar status
if (timer.isActive()) {
    uint32_t remaining = timer.getRemainingTime();
    uint8_t progress = timer.getProgress();  // 0-100%
}

if (timer.hasCompleted()) {
    Serial.println("Timer completado!");
}
```

#### Exemplos Práticos:

**Exemplo 1: Sequência de eventos com timer**
```cpp
ScalableTimer timer1(1000);  // 1 segundo

void onStep1Complete() {
    digitalWrite(OUTPUT1, LOW);
    // Iniciar próximo timer para próxima etapa
}

timer1.onTimerComplete(onStep1Complete);
timer1.start();
```

**Exemplo 2: Monitoramento com barra de progresso**
```cpp
if (button.edgeDetected()) {
    timer.start(3000);  // 3 segundos
}

timer.update();

if (timer.isActive()) {
    Serial.print("Progresso: ");
    Serial.print(timer.getProgress());
    Serial.println("%");
}
```

**Exemplo 3: Timeout automático**
```cpp
ScalableTimer processTimer(10000);  // 10 segundos

if (iniciouProcesso) {
    processTimer.start();
}

processTimer.update();

if (processTimer.hasCompleted()) {
    Serial.println("Processo expirou!");
    desativarProcesso();
}
```

---

## 🔧 Exemplo Completo - Sistema Integrado

```cpp
#include <Arduino.h>
#include "ButtonStateManager.h"

// === PINOS ===
#define BUTTON1 2
#define BUTTON2 3
#define BUTTON3 4
#define OUTPUT1 6
#define OUTPUT2 7

// === OBJETOS ===
Button btn1(BUTTON1, HIGH, 20, FALLING);
Button btn2(BUTTON2, HIGH, 20, FALLING);
Button btn3(BUTTON3, HIGH, 20, RISING);

OutputTimer out1(OUTPUT1, 500, 1000);  // Liga em 0.5s, desliga em 1s
OutputTimer out2(OUTPUT2, 0, 0, true);  // Desliga ao soltar

LogicGate systemLogic(2, AND_MODE);
PriorityOutput safeguard(7, 0, 1, 1);
ScalableTimer systemTimer(0);

void setup() {
    Serial.begin(9600);
}

void loop() {
    // Atualizar botões
    btn1.update();
    btn2.update();
    btn3.update();
    
    // === Lógica Combinada ===
    systemLogic.setInput(0, btn1.getState() == LOW);
    systemLogic.setInput(1, btn2.getState() == LOW);
    
    // Saída 1: Controlada por lógica AND
    out1.trigger(systemLogic.evaluate());
    out1.update();
    
    // Saída 2: Com prioridade (btn3 tem prioridade)
    safeguard.update(btn1.getState() == LOW, btn3.getState() == LOW);
    
    // Timer disparado por borda de subida do btn3
    if (btn3.edgeDetected()) {
        systemTimer.start(5000);
    }
    systemTimer.update();
    
    delay(10);
}
```

---

## 📊 Tabela de Referência Rápida

| Classe | Uso | Escalabilidade |
|--------|-----|-----------------|
| Button | Detecção de botões | Uma por botão |
| OutputTimer | Temporizar saídas | Uma por saída |
| LogicGate | Combinar entradas | Uma por lógica |
| PriorityOutput | Prioridade entre 2 entrada | Uma por saída |
| ScalableTimer | Contagem regressiva | Múltiplas independentes |

---

## 💡 Dicas Importantes

1. **Sempre chamar `update()` no loop** - Sem isso, nada funciona
2. **Botões com PULLUP:** Estado pressionado = LOW
3. **Delays em milisegundos** - 1000 = 1 segundo
4. **Múltiplas instâncias independentes** - Crie quantas precisar
5. **Mude configurações em tempo de execução** - Use setters

---

## 🚀 Próximos Passos

- Implemente o sistema conforme sua aplicação
- Teste cada componente isoladamente
- Combine componentes para criar lógica complexa
- Use estados de máquina (switch/case) para sequências
