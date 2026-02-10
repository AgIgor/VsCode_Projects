#include <Arduino.h>

#define PIN_IGNICAO 13
#define PIN_PORTA 14
#define PIN_TRAVA 25
#define PIN_DESTRAVA 26

#define PIN_FAROL 16
#define PIN_DRL 17
#define PIN_LUZ_TETO 18
#define PIN_LUZ_PE 19

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

enum eventos {
  LIGOU,
  DESLIGOU,
  ABRIU,
  FECHOU,
  TRAVOU,
  DESTRAVOU
};

char listaEventos[];

const int totalBotoes = sizeof(botoes) / sizeof(botoes[0]);

void botaoPressionou(int id) {

  switch (id){
    case 0: // ignição 
        Serial.print(btNames(id));
        Serial.println(" ligou");

        listaEventos.push_back(LIGOU);
    break;

    case 1: // porta
        Serial.print(btNames(id));
        Serial.println(" abriu");
    break;

    case 2: // trava
        Serial.print(btNames(id));
        Serial.println(" travou");
    break;

    case 3: // destrava
        Serial.print(btNames(id));
        Serial.println(" destravou");
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
}

void loop() {
  readButtons();
  delay(50);


}

