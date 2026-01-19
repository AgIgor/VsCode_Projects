#include "IOManager.h"

// Entradas com pull-up interno. Pins escolhidos fora dos strappings principais.
const uint8_t INPUT_PINS[INPUTS_COUNT] = {13, 14, 25, 26};
// Saídas digitais nível HIGH.
const uint8_t OUTPUT_PINS[OUTPUTS_COUNT] = {16, 17, 18, 19, 21, 22};

IOManager ioManager;

IOManager::IOManager() {
    memset(inputs, false, sizeof(inputs));
    memset(outputs, false, sizeof(outputs));
}

void IOManager::init() {
    // Configurar entradas com pull-up
    for (int i = 0; i < INPUTS_COUNT; i++) {
        pinMode(INPUT_PINS[i], INPUT_PULLUP);
    }
    
    // Configurar saídas
    for (int i = 0; i < OUTPUTS_COUNT; i++) {
        pinMode(OUTPUT_PINS[i], OUTPUT);
        digitalWrite(OUTPUT_PINS[i], LOW);
        outputs[i] = false;
    }
}

void IOManager::scanInputs() {
    for (int i = 0; i < INPUTS_COUNT; i++) {
        // Invertido porque é pull-up (HIGH = desligado)
        inputs[i] = !digitalRead(INPUT_PINS[i]);
    }
}

void IOManager::updateOutputs() {
    for (int i = 0; i < OUTPUTS_COUNT; i++) {
        digitalWrite(OUTPUT_PINS[i], outputs[i] ? HIGH : LOW);
    }
}

bool IOManager::getInput(uint8_t index) {
    if (index >= INPUTS_COUNT) return false;
    return inputs[index];
}

void IOManager::setOutput(uint8_t index, bool state) {
    if (index >= OUTPUTS_COUNT) return;
    outputs[index] = state;
}

bool IOManager::getOutput(uint8_t index) {
    if (index >= OUTPUTS_COUNT) return false;
    return outputs[index];
}

void IOManager::getAllInputs(bool* buffer) {
    memcpy(buffer, inputs, INPUTS_COUNT * sizeof(bool));
}

void IOManager::getAllOutputs(bool* buffer) {
    memcpy(buffer, outputs, OUTPUTS_COUNT * sizeof(bool));
}

void IOManager::setAllOutputs(const bool* buffer) {
    memcpy(outputs, buffer, OUTPUTS_COUNT * sizeof(bool));
}
