#pragma once

#include <Arduino.h>

constexpr uint8_t INPUTS_COUNT = 4;
constexpr uint8_t OUTPUTS_COUNT = 6;

// Pinos escolhidos para evitar conflitos de boot e permitir pull-up interno.
extern const uint8_t INPUT_PINS[INPUTS_COUNT];
extern const uint8_t OUTPUT_PINS[OUTPUTS_COUNT];

class IOManager {
public:
    IOManager();

    void init();
    void scanInputs();
    void updateOutputs();

    bool getInput(uint8_t index);
    void getAllInputs(bool* buffer);

    void setOutput(uint8_t index, bool state);
    bool getOutput(uint8_t index);
    void getAllOutputs(bool* buffer);
    void setAllOutputs(const bool* buffer);

private:
    bool inputs[INPUTS_COUNT];
    bool outputs[OUTPUTS_COUNT];
};

extern IOManager ioManager;
