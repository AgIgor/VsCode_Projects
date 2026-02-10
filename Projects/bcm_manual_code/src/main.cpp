#include <Arduino.h>

#define PIN_IGNICAO 13
#define PIN_PORTA 14
#define PIN_TRAVA 25
#define PIN_DESTRAVA 26

#define PIN_FAROL 16
#define PIN_DRL 17
#define PIN_LUZ_TETO 18
#define PIN_LUZ_PE 19


class countDelay {
  private:
    unsigned long startTime;
    unsigned long delayTime;
    bool active;

  public:
    countDelay() : active(false) {}

    void start(unsigned long delay) {
      delayTime = delay;
      startTime = millis();
      active = true;
    }

    bool hasCompleted() {
      if (!active) return false;
      return (millis() - startTime) >= delayTime;
    }

    void stop() {
      active = false;
    }
};


struct Botao {
  uint8_t pino;
  bool estadoAtual;
  bool estadoAnterior;
};
Botao botoes[] = {
  {PIN_IGNICAO, HIGH, HIGH},
  {PIN_PORTA, HIGH, HIGH},
  {PIN_TRAVA, HIGH, HIGH},
  {PIN_DESTRAVA, HIGH, HIGH}
};
const char* btNames(int id) {
  switch (id) {
    case 0: return "Ignicao";
    case 1: return "Porta";
    case 2: return "Trava";
    case 3: return "Destrava";
    default: return "null";
  }
}

struct Saida {
  uint8_t pino;
  bool estadoAtual;
  bool estadoAnterior;
  int toOnDelay; // Tempo para ligar após acionamento (ms)
  int toOffDelay; // Tempo para desligar após acionamento (ms)
};
Saida saidas[] = {
  {PIN_FAROL, LOW, LOW, 0, 0},
  {PIN_DRL, LOW, LOW, 0, 0},
  {PIN_LUZ_TETO, LOW, LOW, 0, 0},
  {PIN_LUZ_PE, LOW, LOW, 0, 0}
};
const char* outNames(int id) {
  switch (id) {
    case 0: return "Farol";
    case 1: return "DRL";
    case 2: return "L_Teto";
    case 3: return "L_Pes";
    default: return "null";
  }
}

enum eventos {
  LIGOU,
  DESLIGOU,
  ABRIU,
  FECHOU,
  TRAVOU,
  DESTRAVOU
};

const int totalBotoes = sizeof(botoes) / sizeof(botoes[0]);
const int totalSaidas = sizeof(saidas) / sizeof(saidas[0]);

countDelay T_on_farol;
countDelay T_off_farol;


void botaoPressionou(int id) {

  switch (id){
    case 0: // ignição 
        Serial.print(btNames(id));
        Serial.println(" ligou");

        //listaEventos.push_back(LIGOU);
    break;

    case 1: // porta
        Serial.print(btNames(id));
        Serial.println(" abriu");
    break;

    case 2: // trava
        Serial.print(btNames(id));
        Serial.println(" travou");

        T_on_farol.start(1000);
        T_off_farol.start(10000);

    break;

    case 3: // destrava
        Serial.print(btNames(id));
        Serial.println(" destravou");

        T_on_farol.start(2000);
        T_off_farol.start(6000);

    break;

    default:
      break;
  }
}

void botaoSoltou(int id) {

  switch (id){
    case 0: // ignição 
        Serial.print(btNames(id));
        Serial.println(" desligou");
    break;

    case 1: // porta
        Serial.print(btNames(id));
        Serial.println(" fechou");
    break;

    default:
      break;
  }
}

void readButtons(){
  for (int i = 0; i < totalBotoes; i++) {

    botoes[i].estadoAtual = digitalRead(botoes[i].pino);
    delay(10); // debounce

    // HIGH → LOW
    if (botoes[i].estadoAnterior == HIGH &&
        botoes[i].estadoAtual == LOW) {

      botaoPressionou(i);
    }

    // LOW → HIGH
    if (botoes[i].estadoAnterior == LOW &&
        botoes[i].estadoAtual == HIGH) {

      botaoSoltou(i);
    }

    botoes[i].estadoAnterior = botoes[i].estadoAtual;
  }
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < totalBotoes; i++) {
    pinMode(botoes[i].pino, INPUT_PULLUP);
    botoes[i].estadoAnterior = digitalRead(botoes[i].pino);
  }
  
  for (int i = 0; i < totalSaidas; i++) {
    pinMode(saidas[i].pino, OUTPUT);
    digitalWrite(saidas[i].pino, saidas[i].estadoAnterior);
  }
}

void loop() {
  readButtons();
  delay(50);

  if (T_on_farol.hasCompleted()) {
    Serial.println("Contador completado! Ligando farol...");
    digitalWrite(PIN_FAROL, HIGH);
    T_on_farol.stop(); // Para evitar múltiplas ativações
  }

  if (T_off_farol.hasCompleted()) {
    Serial.println("Contador completado! Desligando farol...");
    digitalWrite(PIN_FAROL, LOW);
    T_off_farol.stop(); // Para evitar múltiplas ativações
  }
}

