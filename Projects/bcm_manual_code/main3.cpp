#include <Arduino.h>

#define PIN_IGNICAO 13
#define PIN_PORTA 14
#define PIN_TRAVA 25
#define PIN_DESTRAVA 26

#define PIN_FAROL 16
#define PIN_DRL 17
#define PIN_LUZ_TETO 18
#define PIN_LUZ_PE 19

/* =========================================================
   SISTEMA LÓGICO TEMPORIZADO (TON / TOFF)
   4 Entradas + 4 Saídas
   Lógica AND / OR com Delay de Liga e Desliga
   ========================================================= */

#define TOTAL_ENTRADAS 4
#define TOTAL_SAIDAS   5

/* ---------------- ENUM LÓGICA ---------------- */

enum LogicaTipo {
  LOGICA_AND,
  LOGICA_OR
};

/* ---------------- STRUCT SAÍDA ---------------- */
enum ModoSaida {
  MODO_TEMPORIZADO, // TON / TOFF
  MODO_LATCH        // Set / Reset
};


struct SaidaTemporizada {

  uint8_t pinoSaida;

  uint8_t mascaraEntradas;
  LogicaTipo logica;

  // Para temporizado
  unsigned long delayOn;
  unsigned long delayOff;

  // Para latch
  uint8_t mascaraSet;
  uint8_t mascaraReset;

  ModoSaida modo;

  bool estadoSaida;
  bool condicaoAtual;

  unsigned long inicioContagem;
};


/* ---------------- PINOS ---------------- */

uint8_t pinosEntradas[TOTAL_ENTRADAS] = {
  PIN_IGNICAO,
  PIN_PORTA,
  PIN_TRAVA,
  PIN_DESTRAVA
};
bool estadosEntradas[TOTAL_ENTRADAS];

/* ---------------- CONFIG SAÍDAS ----------------
   Mascara em binário:

   Entrada 0 → 0001
   Entrada 1 → 0010
   Entrada 2 → 0100
   Entrada 3 → 1000
------------------------------------------------ */

SaidaTemporizada saidas[TOTAL_SAIDAS] = {

  /* -----------------------------------------
     Saída 0 → TEMPORIZADA
     AND entre entrada 0
  ----------------------------------------- */
  {
    PIN_DRL,
    0b00000001, LOGICA_AND,
    0, 3000,
    0, 0,
    MODO_TEMPORIZADO,
    false, false, 0
  },
  /* -----------------------------------------
     Saída 0 → TEMPORIZADA
     AND entre entrada 0 e 1
  ----------------------------------------- */
  {
    PIN_FAROL,
    0b00000011, LOGICA_AND,
    500, 5000,
    0, 0,
    MODO_TEMPORIZADO,
    false, false, 0
  },

  /* -----------------------------------------
     Saída 1 → LATCH
     Set = entrada 2
     Reset = entrada 3
  ----------------------------------------- */
  {
    PIN_DRL,
    0, LOGICA_AND,
    0, 0,
    0b00000100, // Set
    0b00001000, // Reset
    MODO_LATCH,
    false, false, 0
  },

  /* -----------------------------------------
     Saída 2 → LATCH múltiplo
     Set = 0 ou 1
     Reset = 2 ou 3
  ----------------------------------------- */
  {
    PIN_LUZ_TETO,
    0, LOGICA_AND,
    0, 0,
    0b00000011,
    0b00001100,
    MODO_LATCH,
    false, false, 0
  },

  /* -----------------------------------------
     Saída 3 → TEMPORIZADA simples
  ----------------------------------------- */
  {
    PIN_LUZ_PE,
    0b00000001, LOGICA_AND,
    1000, 1000,
    0, 0,
    MODO_TEMPORIZADO,
    false, false, 0
  }
};


/* =========================================================
   SETUP
   ========================================================= */

void setup() {

  Serial.begin(115200);

  // Configura entradas
  for (int i = 0; i < TOTAL_ENTRADAS; i++) {
    pinMode(pinosEntradas[i], INPUT_PULLUP);
  }

  // Configura saídas
  for (int i = 0; i < TOTAL_SAIDAS; i++) {
    pinMode(saidas[i].pinoSaida, OUTPUT);
    digitalWrite(saidas[i].pinoSaida, LOW);
  }
}

/* =========================================================
   LEITURA ENTRADAS
   ========================================================= */

void lerEntradas() {

  for (int i = 0; i < TOTAL_ENTRADAS; i++) {

    // Inverte por causa do pullup
    estadosEntradas[i] =
      !digitalRead(pinosEntradas[i]);
  }
}

/* =========================================================
   MOTOR LÓGICO
   ========================================================= */

bool avaliarLogica(uint8_t mascara,
                   LogicaTipo tipo) {

  bool resultado =
    (tipo == LOGICA_AND) ? true : false;

  for (int i = 0; i < TOTAL_ENTRADAS; i++) {

    if (mascara & (1 << i)) {

      if (tipo == LOGICA_AND)
        resultado &= estadosEntradas[i];
      else
        resultado |= estadosEntradas[i];
    }
  }

  return resultado;
}

/* =========================================================
   PROCESSAMENTO TON / TOFF
   ========================================================= */

void processarSaidas() {

  unsigned long agora = millis();

  for (int i = 0; i < TOTAL_SAIDAS; i++) {

    /* =========================================
       MODO TEMPORIZADO (TON / TOFF)
       ========================================= */
    if (saidas[i].modo == MODO_TEMPORIZADO) {

      bool condicao = avaliarLogica(
        saidas[i].mascaraEntradas,
        saidas[i].logica
      );

      if (condicao != saidas[i].condicaoAtual) {
        saidas[i].condicaoAtual = condicao;
        saidas[i].inicioContagem = agora;
      }

      // Delay ON
      if (condicao && !saidas[i].estadoSaida) {

        if (agora - saidas[i].inicioContagem
            >= saidas[i].delayOn) {

          saidas[i].estadoSaida = true;
        }
      }

      // Delay OFF
      if (!condicao && saidas[i].estadoSaida) {

        if (agora - saidas[i].inicioContagem
            >= saidas[i].delayOff) {

          saidas[i].estadoSaida = false;
        }
      }
    }

    /* =========================================
       MODO LATCH (SET / RESET)
       ========================================= */
    else if (saidas[i].modo == MODO_LATCH) {

      bool setCond = avaliarLogica(
        saidas[i].mascaraSet,
        LOGICA_OR
      );

      bool resetCond = avaliarLogica(
        saidas[i].mascaraReset,
        LOGICA_OR
      );

      if (setCond)
        saidas[i].estadoSaida = true;

      if (resetCond)
        saidas[i].estadoSaida = false;
    }

    digitalWrite(
      saidas[i].pinoSaida,
      saidas[i].estadoSaida
    );
  }
}


/* =========================================================
   LOOP PRINCIPAL
   ========================================================= */

void loop() {

  lerEntradas();
  processarSaidas();
}
