#include <Arduino.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#ifdef __AVR_ATtiny85__
  #define PIN_SETA        0
  #define PIN_IGNICAO     1
  #define PIN_FAROL       2
  #define PIN_RELE_SETA   3
#elif __AVR_ATmega328P__
  #define PIN_SETA        4  
  #define PIN_IGNICAO     7  
  #define PIN_FAROL       13 
  #define PIN_RELE_SETA   12 
#endif




#define JANELA_PISCADA       1200
#define TEMPO_FOLLOW_1       5000
#define TEMPO_FOLLOW_2       6000
#define TEMPO_POS_IGNICAO    7000

#define TEMPO_MIN_RELE_SETA  2500
#define TEMPO_SETA_INATIVA   3000

unsigned long agora;

unsigned long timerFarol = 0;
unsigned long tempoFarol = 0;

unsigned long timerPosIgnicao = 0;
bool posIgnicaoAtivo = false;

unsigned long ultimaPiscada = 0;
unsigned long ultimaAtividadeSeta = 0;
unsigned long timerReleSeta = 0;

bool farolLigado = false;
bool followMeAtivo = false;

bool ignicaoAtual = false;
bool ignicaoAnterior = false;

bool setaAtual = false;
bool setaAnterior = false;

bool releSetaAtivo = false;

int contPiscadas = 0;

void setup() {

  cli();               // desabilita interrupções
  wdt_reset();
  wdt_disable();       // garante WDT desligado no boot
  
  pinMode(PIN_SETA, INPUT_PULLUP);
  pinMode(PIN_IGNICAO, INPUT_PULLUP);
  pinMode(PIN_FAROL, OUTPUT);
  pinMode(PIN_RELE_SETA, OUTPUT);

  digitalWrite(PIN_FAROL, LOW);
  digitalWrite(PIN_RELE_SETA, LOW);

  wdt_enable(WDTO_1S); // watchdog 1 segundo
  sei();               // reabilita interrupções
}

void loop() {
  delay(150);
  wdt_reset();   // alimenta o watchdog
  agora = millis();

  ignicaoAtual = !digitalRead(PIN_IGNICAO);
  setaAtual = !digitalRead(PIN_SETA);

  /* ===== BORDA DA IGNIÇÃO ===== */
  if (ignicaoAtual && !ignicaoAnterior) {
    // ignição ligou
    digitalWrite(PIN_FAROL, HIGH);
    farolLigado = true;

    followMeAtivo = false;
    posIgnicaoAtivo = false;
    tempoFarol = 0;
  }

  if (!ignicaoAtual && ignicaoAnterior) {
    // ignição desligou
    timerPosIgnicao = agora;
    posIgnicaoAtivo = true;
  }

  /* ===== DRL ===== */
  if (ignicaoAtual) {
    digitalWrite(PIN_FAROL, HIGH);
    farolLigado = true;
  }

  /* ===== TEMPO PÓS-IGNIÇÃO ===== */
  if (posIgnicaoAtivo) {
    digitalWrite(PIN_FAROL, HIGH);
    farolLigado = true;

    if (agora - timerPosIgnicao >= TEMPO_POS_IGNICAO) {
      posIgnicaoAtivo = false;
      if (!followMeAtivo) {
        digitalWrite(PIN_FAROL, LOW);
        farolLigado = false;
      }
    }
  }

  /* ===== FOLLOW ME (somente com ignição desligada) ===== */
  if (!ignicaoAtual) {

    if (setaAtual && !setaAnterior) {
      contPiscadas++;
      ultimaPiscada = agora;
    }

    if (contPiscadas > 0 && (agora - ultimaPiscada > JANELA_PISCADA)) {

      if (contPiscadas == 1) tempoFarol = TEMPO_FOLLOW_1;
      else if (contPiscadas == 2) tempoFarol = TEMPO_FOLLOW_2;
      else tempoFarol = 0;

      if (tempoFarol > 0) {
        digitalWrite(PIN_FAROL, HIGH);
        farolLigado = true;
        followMeAtivo = true;
        timerFarol = agora;
      }

      contPiscadas = 0;
    }

    if (followMeAtivo && (agora - timerFarol >= tempoFarol)) {
      followMeAtivo = false;
      tempoFarol = 0;

      if (!posIgnicaoAtivo) {
        digitalWrite(PIN_FAROL, LOW);
        farolLigado = false;
      }
    }
  }

  /* ===== CONTROLE DO RELÉ DAS SETAS (TRAVADO) ===== */
  if (setaAtual && !setaAnterior) {
    ultimaAtividadeSeta = agora;

    if (!releSetaAtivo) {
      digitalWrite(PIN_RELE_SETA, HIGH);
      releSetaAtivo = true;
      timerReleSeta = agora;
    }
  }

  if (releSetaAtivo) {
    bool tempoMinOk = (agora - timerReleSeta) >= TEMPO_MIN_RELE_SETA;
    bool setaParada = (agora - ultimaAtividadeSeta) >= TEMPO_SETA_INATIVA;

    if (tempoMinOk && setaParada) {
      digitalWrite(PIN_RELE_SETA, LOW);
      releSetaAtivo = false;
    }
  }

  ignicaoAnterior = ignicaoAtual;
  setaAnterior = setaAtual;

  /* ===== FAIL-SAFE FINAL ===== */
  if (!ignicaoAtual && !followMeAtivo && !posIgnicaoAtivo) {
    digitalWrite(PIN_FAROL, LOW);
    farolLigado = false;
  }

}
