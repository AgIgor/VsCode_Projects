#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "IOManager.h"

// Tipos de blocos suportados.
enum class BlockType : uint8_t {
    CONTACT_NO,
    CONTACT_NC,
    COIL,
    AND,
    OR,
    NOT,
    TIMER_ON,
    TIMER_OFF,
    LATCH_SET,
    LATCH_RESET,
    CONST_TRUE,
    CONST_FALSE
};

// Descrição estática do bloco vinda do JSON.
struct BlockDef {
    BlockType type;
    int8_t inA;          // Índice do bloco de entrada A (-1 quando não usado)
    int8_t inB;          // Índice do bloco de entrada B (-1 quando não usado)
    uint8_t ioIndex;     // Índice de I/O (entrada/saída) quando aplicável
    uint32_t delayMs;    // Para temporizadores
};

// Estado em tempo de execução do bloco (timers/latches).
struct BlockRuntime {
    bool value;
    bool latched;
    uint32_t timerStart;
    bool timerActive;
};

constexpr uint8_t MAX_BLOCKS = 64;

class LadderEngine {
public:
    LadderEngine();

    void init();
    void clearProgram();

    // Carrega programa de JSON (string) e opcionalmente persiste.
    bool loadFromJson(const String& json, bool persist = false);
    bool loadFromStorage();
    bool eraseStorage();
    String serializeProgram() const;

    void tick();
    void setCycleMs(uint16_t periodMs);
    uint16_t getCycleMs() const { return cycleMs; }

private:
    bool parseDocument(const JsonDocument& doc);
    bool loadProgramInternal(File& f);
    bool saveProgramInternal(const String& json);
    bool computeBlock(uint8_t idx, uint32_t now);

    BlockDef blocks[MAX_BLOCKS];
    BlockRuntime runtime[MAX_BLOCKS];
    uint8_t blockCount;
    uint16_t cycleMs;
};

extern LadderEngine ladderEngine;
