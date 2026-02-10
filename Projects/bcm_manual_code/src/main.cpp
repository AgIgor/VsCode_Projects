#include <Arduino.h>
#include <ButtonStateManager.h>

// ==================== CONFIGURAÇÃO DE PINOS ====================
#define BUTTON1_PIN 13
#define BUTTON2_PIN 14
#define BUTTON3_PIN 25
#define BUTTON4_PIN 26

#define OUTPUT1_PIN 16
#define OUTPUT2_PIN 17
#define OUTPUT3_PIN 18
#define OUTPUT4_PIN 19

// ==================== INSTÂNCIAS GLOBAIS ====================

// Botões com detecção de borda (configurável)
Button button1(BUTTON1_PIN, HIGH, 20, EDGE_FALLING);  // Borda de descida
Button button2(BUTTON2_PIN, HIGH, 20, EDGE_RISING);   // Borda de subida
Button button3(BUTTON3_PIN, HIGH, 20, EDGE_FALLING);
Button button4(BUTTON4_PIN, HIGH, 20, EDGE_FALLING);

// Saídas com temporização escalável
// OutputTimer(pin, delayOn, delayOff, delayOffOnRelease)
// delayOn = tempo para ligar após acionamento
// delayOff = tempo para desligar após acionamento
// delayOffOnRelease = true: inicia contagem off apenas ao soltar botão

OutputTimer output1(OUTPUT1_PIN, 1000, 2000, false);    // Liga em 1s, desliga em 2s
OutputTimer output2(OUTPUT2_PIN, 0, 3000, false);       // Liga imediato, desliga em 3s
OutputTimer output3(OUTPUT3_PIN, 500, 0, true);         // Liga em 0.5s, desliga ao soltar
OutputTimer output4(OUTPUT4_PIN, 500, 500, false);      // Liga em 0.5s, desliga em 0.5s

// Sistema de lógica (AND/OR)
LogicGate logicGate1(2, GATE_AND);  // 2 inputs, modo AND
LogicGate logicGate2(2, GATE_OR);   // 2 inputs, modo OR

// Saída com prioridade
// PriorityOutput(pin, primaryInput, secondaryInput, priorityLevel)
// priorityLevel: 1 = primary tem prioridade, 0 = secondary tem prioridade
PriorityOutput priorityOutput1(OUTPUT1_PIN, 0, 1, 1);  // Primary (input 1) tem prioridade

// Temporizador escalável
ScalableTimer globalTimer(0);

// ==================== CALLBACKS ====================
void onTimerComplete() {
  Serial.println("Timer completado!");
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(9600);
  delay(1000);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  Serial.println("Sistema de Controle de Botões Iniciado!");
  Serial.println("Recursos disponíveis:");
  Serial.println("1. Detecção de borda (rising/falling)");
  Serial.println("2. Temporização de saídas (on-delay/off-delay)");
  Serial.println("3. Lógica AND/OR");
  Serial.println("4. Saídas com prioridade");
  Serial.println("5. Timer escalável");
  Serial.println("6. Latch (mantenha pressionado)");
  
  // Configurar callback do timer
  globalTimer.onTimerComplete(onTimerComplete);
}

// ==================== LOOP PRINCIPAL ====================
void loop() {
  // Atualizar leitura dos botões
  button1.update();
  button2.update();
  button3.update();
  button4.update();
  
  // === EXEMPLO 1: Controle simples com temporização ===
  // Botão 1 controla saída 1 com delay on/off
  if (button1.edgeDetected()) {
    Serial.println("Botão 1 pressionado!");
  }
  output1.trigger(button1.getState() == LOW);  // LOW = pressionado
  output1.update();
  
  // === EXEMPLO 2: Saída com delay on=0 (liga instantâneo) ===
  output2.trigger(button2.getState() == LOW);
  output2.update();
  
  // === EXEMPLO 3: Saída com delay off apenas ao soltar ===
  output3.trigger(button3.getState() == LOW);
  output3.update();
  
  // === EXEMPLO 4: Controle com Lógica AND/OR ===
  logicGate1.setInput(0, button1.getState() == LOW);
  logicGate1.setInput(1, button2.getState() == LOW);
  
  logicGate2.setInput(0, button3.getState() == LOW);
  logicGate2.setInput(1, button4.getState() == LOW);
  
  output4.trigger(logicGate1.evaluate());  // Usa lógica AND
  output4.update();
  
  // === EXEMPLO 5: Saída com Prioridade ===
  priorityOutput1.update(button1.getState() == LOW, button2.getState() == LOW);
  
  // === EXEMPLO 6: Timer Escalável ===
  if (button4.edgeDetected()) {
    Serial.println("Iniciando timer de 5 segundos...");
    globalTimer.start(5000);
  }
  
  globalTimer.update();
  
  if (globalTimer.isActive()) {
    Serial.print("Tempo restante: ");
    Serial.print(globalTimer.getRemainingTime());
    Serial.print("ms - Progresso: ");
    Serial.print(globalTimer.getProgress());
    Serial.println("%");
    delay(500);
  }
  
  delay(10);  // Loop de 10ms para responsividade
}

// ==================== EXEMPLOS DE USO AVANÇADO ====================

/*
// EXEMPLO: Múltiplas saídas com lógica complexa
void exemploLogicaComplexaComLatch() {
  // Criar portão AND com 4 entradas
  LogicGate systemLogic(4, AND_MODE);
  
  // Todas as 4 entradas devem estar ativas para ligar a saída
  systemLogic.setInput(0, button1.getState() == LOW);
  systemLogic.setInput(1, button2.getState() == LOW);
  systemLogic.setInput(2, button3.getState() == LOW);
  systemLogic.setInput(3, button4.getState() == LOW);
  
  if (systemLogic.evaluate()) {
    digitalWrite(OUTPUT1_PIN, HIGH);
  } else {
    digitalWrite(OUTPUT1_PIN, LOW);
  }
}

// EXEMPLO: Temporizador para sequência de eventos
void exemploSequenciaComTimer() {
  static uint8_t sequenceStep = 0;
  static ScalableTimer stepTimer(1000);
  
  switch(sequenceStep) {
    case 0:
      if (button1.edgeDetected()) {
        Serial.println("Iniciando sequência...");
        stepTimer.start(1000);
        sequenceStep = 1;
      }
      break;
      
    case 1:
      stepTimer.update();
      if (stepTimer.hasCompleted()) {
        digitalWrite(OUTPUT1_PIN, HIGH);
        stepTimer.start(500);
        stepTimer.onTimerComplete([]() {
          digitalWrite(OUTPUT1_PIN, LOW);
        });
        stepTimer.update();
        stepTimer.stop();
        sequenceStep = 2;
      }
      break;
      
    case 2:
      Serial.println("Sequência completa!");
      sequenceStep = 0;
      break;
  }
}

// EXEMPLO: Alternar entre Rising e Falling edge
void exemploMudacaoDeBorda() {
  static uint32_t lastChange = 0;
  
  // Trocar borda a cada 5 segundos
  if (millis() - lastChange > 5000) {
    if (button1.getState() == HIGH) {
      button1.setEdgeType(FALLING);
      Serial.println("Detectando borda de descida");
    } else {
      button1.setEdgeType(RISING);
      Serial.println("Detectando borda de subida");
    }
    lastChange = millis();
  }
}

// EXEMPLO: Sistema de latch (mantenha ativo)
class LatchSystem {
  bool isLatched = false;
  uint32_t latchTime = 0;
  uint32_t maxLatchDuration = 5000;  // 5 segundos máximo
  
  public:
    void update(bool input, uint8_t outputPin) {
      if (input && !isLatched) {
        // Ativar latch
        isLatched = true;
        latchTime = millis();
        digitalWrite(outputPin, HIGH);
        Serial.println("Latch ativado!");
      }
      
      if (isLatched) {
        // Verificar timeout do latch
        if (millis() - latchTime > maxLatchDuration) {
          isLatched = false;
          digitalWrite(outputPin, LOW);
          Serial.println("Latch desativado por timeout");
        }
      }
    }
    
    bool isActive() {
      return isLatched;
    }
};
*/