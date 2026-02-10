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

struct statusCarro {
  bool ignicao;
  bool porta;
  bool trava;
  bool destrava;
  bool farol;
  bool drl;
  bool luzTeto;
  bool luzPe;
  bool saiu;
  bool entrou;
};

statusCarro carro = {false, false, false, false, false, false, false, false, false, false};
// const char* statusNames(int id) {
//   switch (id) {
//     case 0: return "Ignicao";
//     case 1: return "Porta";
//     case 2: return "Trava";
//     case 3: return "Destrava";
//     case 4: return "Farol";
//     case 5: return "DRL";
//     case 6: return "Luz Teto";
//     case 7: return "Luz Pe";
//     case 8: return "Saiu";
//     case 9: return "Entrou";
//     default: return "null";
//   }
// }


struct Entrada {
  uint8_t pino;
  bool estadoAtual;
  bool estadoAnterior;
};
Entrada entradas[] = {
  {PIN_IGNICAO, HIGH, HIGH},
  {PIN_PORTA, HIGH, HIGH},
  {PIN_TRAVA, HIGH, HIGH},
  {PIN_DESTRAVA, HIGH, HIGH}
};
// const char* entradaNomes(int id) {
//   switch (id) {
//     case 0: return "Ignicao";
//     case 1: return "Porta";
//     case 2: return "Trava";
//     case 3: return "Destrava";
//     default: return "null";
//   }
// }

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
// const char* saidaNomes(int id) {
//   switch (id) {
//     case 0: return "Farol";
//     case 1: return "DRL";
//     case 2: return "L_Teto";
//     case 3: return "L_Pes";
//     default: return "null";
//   }
// }

const int totalEntradas = sizeof(entradas) / sizeof(entradas[0]);
const int totalSaidas = sizeof(saidas) / sizeof(saidas[0]);

countDelay T_on_farol;
countDelay T_off_farol;

countDelay T_on_drl;
countDelay T_off_drl;

countDelay T_on_luzTeto;
countDelay T_off_luzTeto;

countDelay T_on_luzPe;
countDelay T_off_luzPe;

void printStatus(){
  Serial.print("Ignicao: "); Serial.print(carro.ignicao ? "ON" : "OFF");
  Serial.print(" | Porta: "); Serial.print(carro.porta ? "ABERTA" : "FECHADA");
  Serial.print(" | Trava: "); Serial.print(carro.trava ? "TRAVADA" : "DESTRAVADA");
  Serial.print(" | Farol: "); Serial.print(carro.farol ? "ON" : "OFF");
  Serial.print(" | DRL: "); Serial.print(carro.drl ? "ON" : "OFF");
  Serial.print(" | Luz Teto: "); Serial.print(carro.luzTeto ? "ON" : "OFF");
  Serial.print(" | Luz Pe: "); Serial.print(carro.luzPe ? "ON" : "OFF");
  Serial.print(" | Ocupado: "); Serial.println(carro.entrou ? "SIM" : "NAO");
}

void loopSaidas(){
    if (T_on_farol.hasCompleted()) {
    Serial.println("Contador completado! Ligando farol...");
    digitalWrite(PIN_FAROL, HIGH);
    carro.farol = true;
    T_on_farol.stop(); // Para evitar múltiplas ativações
  }

  if (T_off_farol.hasCompleted()) {
    Serial.println("Contador completado! Desligando farol...");
    digitalWrite(PIN_FAROL, LOW);
    carro.farol = false;
    T_off_farol.stop(); // Para evitar múltiplas ativações
  }
}

void processaStatus(){
  // Exemplo de processamento: Se porta aberta e ignição desligada, ligar DRL
  // if (carro.porta && !carro.ignicao) {
  //   digitalWrite(PIN_DRL, HIGH);
  //   carro.drl = true;
  // } else {
  //   digitalWrite(PIN_DRL, LOW);
  //   carro.drl = false;
  // }

  if(carro.ignicao == false and carro.trava == true){ //se ignição desligada e trava ativada, ligar farol por 1s e desligar após 10s
    T_on_farol.start(1000); //ligar farol após 1s
    T_off_farol.start(10000); //desligar farol após 10s
  };

  if(carro.ignicao == false and carro.destrava == true){ //se ignição desligada e destrava ativada, ligar farol por 2s e desligar após 6s
    T_on_farol.start(2000); //ligar farol após 2s
    T_off_farol.start(6000); //desligar farol após 6s
  }
  
  if(carro.ignicao == true){ //se ignição ligada, desligar farol imediatamente
    T_on_farol.stop();
    T_off_farol.stop();
    Serial.println("Contadores de farol parados.");
  };

  // if(entradas){

  // }


  printStatus();
}

void entradaLigou(int id) {
  switch (id){
    case 0: // ignição 
        carro.ignicao = true;
    break;

    case 1: // porta
        carro.porta = true;
    break;

    case 2: // trava
        carro.trava = true; 
        carro.destrava = false; 
    break;

    case 3: // destrava
        carro.destrava = true;
        carro.trava = false;
    break;

    default:
      break;
  }

  processaStatus();
}

void entradaDesligou(int id) {
  switch (id){
    case 0: // ignição 
        carro.ignicao = false;
    break;

    case 1: // porta
        carro.porta = false;
    break;

    default:
      break;
  }
  processaStatus();
}

void lerEntradas(){
  for (int i = 0; i < totalEntradas; i++) {

    entradas[i].estadoAtual = digitalRead(entradas[i].pino);
    delay(10); // debounce

    // HIGH → LOW
    if (entradas[i].estadoAnterior == HIGH &&
        entradas[i].estadoAtual == LOW) {

      entradaLigou(i);
    }

    // LOW → HIGH
    if (entradas[i].estadoAnterior == LOW &&
        entradas[i].estadoAtual == HIGH) {

      entradaDesligou(i);
    }

    entradas[i].estadoAnterior = entradas[i].estadoAtual;
  }
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < totalEntradas; i++) {
    pinMode(entradas[i].pino, INPUT_PULLUP);
    entradas[i].estadoAnterior = digitalRead(entradas[i].pino);
  }
  
  for (int i = 0; i < totalSaidas; i++) {
    pinMode(saidas[i].pino, OUTPUT);
    digitalWrite(saidas[i].pino, saidas[i].estadoAnterior);
  }
}

void loop() {
  lerEntradas();
  delay(50);
  loopSaidas();
}

