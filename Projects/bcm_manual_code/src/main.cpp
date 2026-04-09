#include <Arduino.h>

#define PIN_IGNICAO 13
#define PIN_PORTA 14
#define PIN_TRAVA 25
#define PIN_DESTRAVA 26

#define PIN_FAROL 16
#define PIN_DRL 17
#define PIN_LUZ_TETO 18
#define PIN_LUZ_PE 19

struct Evento {
  uint8_t entrada;     // 0 a 3
  bool estado;         // HIGH ou LOW
  unsigned long tempo; // millis()
};

#define MAX_EVENTOS 50

Evento logEventos[MAX_EVENTOS];
int indiceGravacao = 0;



void registrarEvento(uint8_t entrada, bool estado) {
  logEventos[indiceGravacao].entrada = entrada;
  logEventos[indiceGravacao].estado  = estado;
  logEventos[indiceGravacao].tempo   = millis();

  indiceGravacao++;
  if (indiceGravacao >= MAX_EVENTOS) {
    indiceGravacao = 0; // volta pro início (circular)
  }
}

void limparLog() {
  for (int i = 0; i < MAX_EVENTOS; i++) {
    logEventos[i].entrada = 255; // valor inválido
    logEventos[i].estado  = 0;
    logEventos[i].tempo   = 0;
  }
}

const int pinos[4] = {PIN_IGNICAO, PIN_PORTA, PIN_TRAVA, PIN_DESTRAVA};

bool estadoAtual[4];
bool estadoAnterior[4];
unsigned long ultimoChangeMs[4];
const unsigned long debounceMs = 50;

void imprimirLog() {
  Serial.println("---- LOG ----");

  for (int i = 0; i < MAX_EVENTOS; i++) {
    Serial.print("Entrada: ");
    Serial.print(logEventos[i].entrada);
    Serial.print(" Estado: ");
    Serial.print(logEventos[i].estado);
    Serial.print(" Tempo: ");
    Serial.println(logEventos[i].tempo);
  }
}

bool verificarSequencia() {

  int etapa = 0;

  for (int i = 0; i < MAX_EVENTOS; i++) {

    Evento e = logEventos[i];
    if (e.entrada == 255) {
      continue;
    }

    switch (etapa) {

      case 0:
        if (e.entrada == 0 && e.estado == HIGH)
          etapa++;
        break;

      case 1:
        if (e.entrada == 1 && e.estado == HIGH)
          etapa++;
        break;

      case 2:
        if (e.entrada == 2 && e.estado == LOW)
          return true;
        break;
    }
  }

  return false;
}

bool detectarSequenciaTempo() {

  Evento e1, e2;
  bool achouE1 = false;

  for (int i = 0; i < MAX_EVENTOS; i++) {

    if (logEventos[i].entrada == 255) {
      continue;
    }

    if (!achouE1) {
      if (logEventos[i].entrada == 0 &&
          logEventos[i].estado == HIGH) {

        e1 = logEventos[i];
        achouE1 = true;
      }
    }
    else {
      if (logEventos[i].entrada == 1 &&
          logEventos[i].estado == HIGH) {

        e2 = logEventos[i];

        unsigned long delta = e2.tempo - e1.tempo;

        if (delta <= 3000) {
          return true; // sequência válida
        }
        else {
          achouE1 = false; // demorou demais, continua procurando
        }
      }
    }
  }

  return false;
}


void setup(){
    Serial.begin(115200);

  limparLog();

    for (int i = 0; i < 4; i++) {
        pinMode(pinos[i], INPUT_PULLUP);
        estadoAnterior[i] = digitalRead(pinos[i]);
    ultimoChangeMs[i] = 0;
    }

}

void loop() {
    for (int i = 0; i < 4; i++) {

    estadoAtual[i] = digitalRead(pinos[i]);

    if (estadoAtual[i] != estadoAnterior[i]) {
      unsigned long agora = millis();
      if (agora - ultimoChangeMs[i] >= debounceMs) {
        registrarEvento(i, estadoAtual[i]);
        estadoAnterior[i] = estadoAtual[i];
        ultimoChangeMs[i] = agora;
      }
    }
    }

    if (verificarSequencia()) {
        Serial.println("Sequência detectada!");
        imprimirLog();
        limparLog(); // FINALIZA
    }

    if (detectarSequenciaTempo()) {
        Serial.println("Sequência detectada no tempo!");
        limparLog(); // FINALIZA
    }
}
