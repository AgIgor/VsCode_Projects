#include <Arduino.h>

const uint8_t saidas[] = {
    PD7,
    PD4,
    PD3,
    PC1,
    PC2,
    PD6,
    PC4,
    PD2
};

const uint8_t numSaidas = sizeof(saidas) / sizeof(saidas[0]);

void setup() {
    for (uint8_t i = 0; i < numSaidas; i++) {
        pinMode(saidas[i], OUTPUT);
        digitalWrite(saidas[i], LOW);
    }

    // pinMode(PD7, INPUT_PULLUP);
    // pinMode(PB4, INPUT);

}

void loop() {
    // for (uint8_t i = 0; i < numSaidas; i++) {

    //     // Apaga todas
    //     for (uint8_t j = 0; j < numSaidas; j++) {
    //         digitalWrite(saidas[j], LOW);

    //     }
    //     delay(100);

    //     digitalWrite(saidas[i], HIGH);

    //     delay(100);
    // }


    // digitalWrite(PD4, (millis() / map(analogRead(PB4), 0, 1023, 5, 3000)) % 2);

    // delay(50);
}
