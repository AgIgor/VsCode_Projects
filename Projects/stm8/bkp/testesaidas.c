#include <Arduino.h>

const uint8_t saidas[] = {
    PD4,
    // PD7,
    PD3,
    PC1,
    PC2,
    PD6,
    PD4,
    PD2

};

const uint8_t entradasLOW[] = {
    PB0,
    PB1,
    PB2,
    // PD7,
    // PD1,
    PB5,
    PB4    
};

const uint8_t entradasHIGH[] = {

    PA2,
    PA1,
    PE5,
    // PD7
    // PD1,

};

const uint8_t numSaidas = sizeof(saidas) / sizeof(saidas[0]);
const uint8_t numEntradasLOW = sizeof(entradasLOW) / sizeof(entradasLOW[0]);
const uint8_t numEntradasHIGH = sizeof(entradasHIGH) / sizeof(entradasHIGH[0]);

void setup() {
    for (uint8_t i = 0; i < numSaidas; i++) {
        pinMode(saidas[i], OUTPUT);
        digitalWrite(saidas[i], LOW);
    }
    for (uint8_t i = 0; i < numEntradasLOW; i++) {
        pinMode(entradasLOW[i], INPUT);

    }
    for (uint8_t i = 0; i < numEntradasHIGH; i++) {
        pinMode(entradasHIGH[i], INPUT);

    }



}

void loop() {
    for (uint8_t i = 0; i < numSaidas; i++) {

        for (uint8_t j = 0; j < numSaidas; j++) {
            digitalWrite(saidas[j], LOW);
            digitalWrite(PD4, HIGH);
        }
        delay(500);

        digitalWrite(saidas[i], HIGH);
        digitalWrite(PD4, LOW);

        delay(500);
    }

    // for (uint8_t i = 0; i < numEntradasLOW; i++) {
    //     if (digitalRead(entradasLOW[i]) == LOW || digitalRead(ADC1_CHANNEL_12) == LOW) {
    //         digitalWrite(PD4, HIGH);
    //         delay(50);
    //         digitalWrite(PD4, LOW);
    //         delay(50);
    //         digitalWrite(PD4, HIGH);
    //         delay(50);
    //         digitalWrite(PD4, LOW);
    //         delay(50);
    //         digitalWrite(PD4, HIGH);
    //         delay(50);
    //         digitalWrite(PD4, LOW);
    //         delay(50);
    //         digitalWrite(PD4, HIGH);
    //         delay(50);
    //         digitalWrite(PD4, LOW);
    //         delay(50);
    //         digitalWrite(PD4, HIGH);
    //     }
    // }
    
    // for (uint8_t i = 0; i < numEntradasHIGH; i++) {
    //     if (digitalRead(entradasHIGH[i]) == HIGH) {
    //         digitalWrite(PD4, HIGH);
    //         delay(50);
    //         digitalWrite(PD4, LOW);
    //         delay(50);
    //         digitalWrite(PD4, HIGH);
    //         delay(50);
    //         digitalWrite(PD4, LOW);
    //         delay(50);
    //         digitalWrite(PD4, HIGH);
    //         delay(50);
    //         digitalWrite(PD4, LOW);
    //         delay(50);
    //         digitalWrite(PD4, HIGH);
    //         delay(50);
    //         digitalWrite(PD4, LOW);
    //         delay(50);
    //         digitalWrite(PD4, HIGH);
    //     }
    // }

    // digitalWrite(PD4, (millis() / 1000) % 2);
    // analogWrite(PD4, map(analogRead(PB4), 0, 1023, 0, 255));




}
