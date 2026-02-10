// =============== EXEMPLOS AVANÇADOS - COPIE E COLE ===============
// Este arquivo contém exemplos prontos para copiar e implementar

#include <Arduino.h>
#include <ButtonStateManager.h>

// ========== EXEMPLO 1: Detecção de Mudança de Estado ==========
// Executa função ao detectar mudança HIGH->LOW ou LOW->HIGH

void exemplo1_DeteccaoMudancaEstado() {
    Button botao(2, HIGH, 20, EDGE_FALLING);  // Detecta mudança de HIGH para LOW
    
    // No setup
    pinMode(13, OUTPUT);
    
    // No loop
    botao.update();
    
    if (botao.edgeDetected()) {
        // Executar função quando mudança detectada
        digitalWrite(13, !digitalRead(13));  // Toggle LED
        Serial.println("Estado mudou!");
    }
}

// ========== EXEMPLO 2: Temporização 4 Saídas com AND/OR ==========
// Controla 4 saídas usando lógica AND e OR

void exemplo2_Temporizacao4Saidas() {
    // Botões
    Button btn1(2, HIGH, 20, EDGE_FALLING);
    Button btn2(3, HIGH, 20, EDGE_FALLING);
    Button btn3(4, HIGH, 20, EDGE_FALLING);
    Button btn4(5, HIGH, 20, EDGE_FALLING);
    
    // Saídas com temporização
    OutputTimer saida1(6, 500, 1000, false);   // Saída 1: on-delay 500ms, off-delay 1000ms
    OutputTimer saida2(7, 1000, 2000, false);  // Saída 2: on-delay 1000ms, off-delay 2000ms
    OutputTimer saida3(8, 0, 1500, false);     // Saída 3: on instantâneo, off-delay 1500ms
    OutputTimer saida4(9, 2000, 500, false);   // Saída 4: on-delay 2000ms, off-delay 500ms
    
    // Lógica AND (todas entradas devem estar ativas)
    LogicGate logicAND(4, GATE_AND);
    
    // Lógica OR (pelo menos uma entrada deve estar ativa)
    LogicGate logicOR(2, GATE_OR);
    
    // No loop
    btn1.update();
    btn2.update();
    btn3.update();
    btn4.update();
    
    // Configurar lógica AND
    logicAND.setInput(0, btn1.getState() == LOW);
    logicAND.setInput(1, btn2.getState() == LOW);
    logicAND.setInput(2, btn3.getState() == LOW);
    logicAND.setInput(3, btn4.getState() == LOW);
    
    // Configurar lógica OR
    logicOR.setInput(0, btn1.getState() == LOW);
    logicOR.setInput(1, btn2.getState() == LOW);
    
    // Aplicar saídas
    saida1.trigger(logicAND.evaluate());  // Saída 1 ativa se TODOS pressionados
    saida1.update();
    
    saida2.trigger(logicOR.evaluate());   // Saída 2 ativa se ALGUM pressionado
    saida2.update();
    
    saida3.trigger(btn3.getState() == LOW);
    saida3.update();
    
    saida4.trigger(btn4.getState() == LOW);
    saida4.update();
}

// ========== EXEMPLO 3: Saída Escalável (Xs ON, Xs OFF) ==========
// Forma simples de definir tempo de ligação e desligação

void exemplo3_SaidaEscalavel() {
    OutputTimer bomba(6, 1000, 2000);  // Liga em 1s, desliga em 2s
    Button botao(2, HIGH, 20, EDGE_FALLING);
    
    // No setup: nada especial
    
    // No loop
    botao.update();
    bomba.trigger(botao.getState() == LOW);
    bomba.update();
    
    // Mudar valores em tempo de execução
    if (botao.edgeDetected()) {
        bomba.setDelays(500, 1500);  // Novo: 500ms on, 1500ms off
    }
}

// ========== EXEMPLO 4: Sistema de Latch ==========
// Mantém saída ligada até receber comando para desligar

class SystemLatch {
private:
    bool isLatched;
    uint8_t outputPin;
    uint32_t maxDuration;  // Timeout automático
    uint32_t latchTime;
    
public:
    SystemLatch(uint8_t pin, uint32_t maxDurationMs = 10000) 
        : outputPin(pin), isLatched(false), maxDuration(maxDurationMs), latchTime(0) {
        pinMode(outputPin, OUTPUT);
        digitalWrite(outputPin, LOW);
    }
    
    // Ligar latch (mantém ligado)
    void activate() {
        if (!isLatched) {
            isLatched = true;
            latchTime = millis();
            digitalWrite(outputPin, HIGH);
            Serial.println("Latch ativado!");
        }
    }
    
    // Desligar latch
    void deactivate() {
        isLatched = false;
        digitalWrite(outputPin, LOW);
        Serial.println("Latch desativado!");
    }
    
    // Chamar no loop para verificar timeout
    void update() {
        if (isLatched && (millis() - latchTime > maxDuration)) {
            deactivate();
            Serial.println("Latch timeout!");
        }
    }
    
    bool isActive() { return isLatched; }
};

// Usar:
void exemplo4_Latch() {
    SystemLatch latch(6, 5000);  // Timeout de 5 segundos
    Button btnLigar(2, HIGH, 20, EDGE_FALLING);
    Button btnDesligar(3, HIGH, 20, EDGE_FALLING);
    
    // Setup: nada especial
    
    // Loop
    btnLigar.update();
    btnDesligar.update();
    latch.update();
    
    if (btnLigar.edgeDetected()) {
        latch.activate();
    }
    
    if (btnDesligar.edgeDetected()) {
        latch.deactivate();
    }
}

// ========== EXEMPLO 5: Uma Entrada Temporizada com AND/OR ==========
// Uma única entrada controla saída com lógica e temporização

void exemplo5_EntradaUnicaTemporizada() {
    Button botao(2, HIGH, 20, EDGE_FALLING);
    OutputTimer saida(6, 2000, 3000);  // On-delay 2s, off-delay 3s
    
    // Lógica (exemplo: combinar com sensor)
    int sensor = A0;
    LogicGate logica(2, GATE_AND);  // E a entrada com sensor
    
    // Loop
    botao.update();
    int sensorValue = digitalRead(sensor);
    
    logica.setInput(0, botao.getState() == LOW);
    logica.setInput(1, sensorValue == HIGH);
    
    saida.trigger(logica.evaluate());
    saida.update();
}

// ========== EXEMPLO 6: Prioridade entre Duas Entradas ==========
// Duas entradas, uma com prioridade sobre a outra

void exemplo6_Prioridade() {
    Button botaoPrincipal(2, HIGH, 20, EDGE_FALLING);
    Button botaoRemoto(3, HIGH, 20, EDGE_FALLING);
    
    // Botão principal (pino 2) tem prioridade
    PriorityOutput motor(6, 0, 1, 1);
    
    // No loop
    botaoPrincipal.update();
    botaoRemoto.update();
    
    motor.update(
        botaoPrincipal.getState() == LOW,  // Input 0 (principal)
        botaoRemoto.getState() == LOW      // Input 1 (remoto)
    );
    
    // Se principal quer ligar, motor liga imediatamente
    // Se principal quer desligar, remoto É IGNORADO
}

// ========== EXEMPLO 7: Contador de Tempo Escalável ==========
// Timer que executa tarefa após X tempo

void exemplo7_ContadorTempoEscalavel() {
    // Timer de 5 segundos
    ScalableTimer timer(5000);
    Button botao(2, HIGH, 20, EDGE_FALLING);
    
    // Callback ao completar
    void onTimerComplete() {
        Serial.println("5 segundos decorridos!");
        digitalWrite(13, HIGH);
    }
    
    // Setup
    timer.onTimerComplete(onTimerComplete);
    pinMode(13, OUTPUT);
    
    // Loop
    botao.update();
    timer.update();
    
    // Iniciar timer ao pressionar botão
    if (botao.edgeDetected()) {
        Serial.println("Iniciando timer...");
        timer.start();
    }
    
    // Monitorar progresso
    if (timer.isActive()) {
        Serial.print("Tempo restante: ");
        Serial.print(timer.getRemainingTime());
        Serial.print("ms - Progresso: ");
        Serial.print(timer.getProgress());
        Serial.println("%");
    }
}

// ========== EXEMPLO 8: Botão Com Contagem Para Ligar ==========
// Pressionar botão inicia contagem: liga em X tempo por X tempo

void exemplo8_BotaoComContagem() {
    Button botao(2, HIGH, 20, EDGE_FALLING);
    OutputTimer bomba(6, 2000, 3000, false);  // Liga em 2s, desliga em 3s
    
    // Loop
    botao.update();
    
    // Se delayOn=0, liga instantâneo
    // Se delayOff=0, desliga ao soltar
    // Exemplo: delay on=1000, delay off=0
    OutputTimer valve(7, 1000, 0);
    valve.trigger(botao.getState() == LOW);
    valve.update();
}

// ========== EXEMPLO 9: Delay OFF Iniciado ao Soltar ==========
// Inicia contagem de desligação apenas ao soltar o botão

void exemplo9_DelayOffAoSoltar() {
    Button botao(2, HIGH, 20, EDGE_FALLING);
    
    // delayOffOnRelease = true: off-delay inicia ao soltar
    OutputTimer pistola(6, 500, 2000, true);
    
    // Loop
    botao.update();
    pistola.trigger(botao.getState() == LOW);
    pistola.update();
    
    // Comportamento:
    // 1. Pressiona botão -> conta 500ms -> liga
    // 2. Solta botão -> inicia contagem de 2000ms -> desliga
}

// ========== EXEMPLO 10: Configurar Rising/Falling Edge ==========
// Escolher se detecta subida ou descida de tensão

void exemplo10_RisingFallingEdge() {
    // Detectar quando botão VEM PARA LOW (queda)
    Button btn1(2, HIGH, 20, EDGE_FALLING);
    
    // Detectar quando botão VAI PARA HIGH (subida)
    Button btn2(3, HIGH, 20, EDGE_RISING);
    
    // Mudar tipo de borda em tempo de execução
    if (algumaCodition) {
        btn1.setEdgeType(EDGE_RISING);  // Agora detecta subida
    }
    
    // Loop
    btn1.update();
    btn2.update();
    
    if (btn1.edgeDetected()) {
        Serial.println("BTN1: Borda detectada (EDGE_FALLING)");
    }
    
    if (btn2.edgeDetected()) {
        Serial.println("BTN2: Borda detectada (EDGE_RISING)");
    }
}

// ========== EXEMPLO 11: Sistema Completo Integrado ==========
// Combina todos os elementos juntos

void exemplo11_SistemaCompleto() {
    // === Entradas ===
    Button manual(2, HIGH, 20, EDGE_FALLING);
    Button remoto(3, HIGH, 20, EDGE_FALLING);
    Button reset(4, HIGH, 20, EDGE_RISING);
    Button emergencia(5, HIGH, 20, EDGE_FALLING);
    
    // === Saídas ===
    OutputTimer motor1(6, 500, 1000);
    OutputTimer motor2(7, 1000, 1500);
    OutputTimer sinal(8, 0, 0);
    
    // === Lógica ===
    LogicGate systemGate(2, GATE_AND);
    PriorityOutput safeMotor(9, 0, 1, 1);  // manual tem prioridade
    SystemLatch emergencyLatch(10, 10000);
    ScalableTimer sequenceTimer(0);
    
    // Loop
    manual.update();
    remoto.update();
    reset.update();
    emergencia.update();
    
    // Processamento
    emergencyLatch.update();
    sequenceTimer.update();
    
    // Check reset
    if (reset.edgeDetected()) {
        emergencyLatch.deactivate();
        sequenceTimer.stop();
    }
    
    // Check emergência
    if (emergencia.edgeDetected()) {
        emergencyLatch.activate();
    }
    
    // Lógica de controle
    systemGate.setInput(0, manual.getState() == LOW);
    systemGate.setInput(1, remoto.getState() == LOW);
    
    // Motores
    motor1.trigger(systemGate.evaluate() && !emergencyLatch.isActive());
    motor1.update();
    
    motor2.trigger(remoto.getState() == LOW && !emergencyLatch.isActive());
    motor2.update();
    
    // Sinal de status
    sinal.trigger(systemGate.evaluate());
    sinal.update();
    
    // Timer para sequência
    if (systemGate.evaluate() && !sequenceTimer.isActive()) {
        sequenceTimer.start(5000);  // 5 segundos
    }
}

// ========== EXEMPLO 12: Máquina de Estados Com Timers ==========
// Estados diferentes com transições e timers

enum State { IDLE, WAITING, ACTIVE, COOLING };

class StateMachine {
private:
    State currentState;
    ScalableTimer stateTimer;
    
public:
    StateMachine() : currentState(IDLE) {}
    
    void update(bool trigger, bool stop) {
        stateTimer.update();
        
        switch(currentState) {
            case IDLE:
                if (trigger) {
                    currentState = WAITING;
                    stateTimer.start(2000);  // Esperar 2 segundos
                    Serial.println("-> WAITING");
                }
                break;
                
            case WAITING:
                if (stateTimer.hasCompleted()) {
                    currentState = ACTIVE;
                    stateTimer.start(5000);  // Ativar por 5 segundos
                    digitalWrite(OUTPUT_PIN, HIGH);
                    Serial.println("-> ACTIVE");
                }
                break;
                
            case ACTIVE:
                if (stop || stateTimer.hasCompleted()) {
                    currentState = COOLING;
                    stateTimer.start(3000);  // Resfriar por 3 segundos
                    digitalWrite(OUTPUT_PIN, LOW);
                    Serial.println("-> COOLING");
                }
                break;
                
            case COOLING:
                if (stateTimer.hasCompleted()) {
                    currentState = IDLE;
                    Serial.println("-> IDLE");
                }
                break;
        }
    }
};

// ========== REFERÊNCIA: ENUM PARA VALORES ==========
/*
EdgeType:
    - EDGE_RISING (1): Detecta mudança de LOW para HIGH
    - EDGE_FALLING (0): Detecta mudança de HIGH para LOW

LogicMode:
    - GATE_AND (0): Todos os inputs devem ser verdadeiros
    - GATE_OR (1): Pelo menos um input deve ser verdadeiro
*/
