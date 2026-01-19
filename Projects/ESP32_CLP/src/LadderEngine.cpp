#include "LadderEngine.h"

LadderEngine ladderEngine;

namespace {

constexpr const char* PROGRAM_PATH = "/program.json";

BlockType blockTypeFromString(const String& t) {
    if (t == "CONTACT_NO") return BlockType::CONTACT_NO;
    if (t == "CONTACT_NC") return BlockType::CONTACT_NC;
    if (t == "COIL") return BlockType::COIL;
    if (t == "AND") return BlockType::AND;
    if (t == "OR") return BlockType::OR;
    if (t == "NOT") return BlockType::NOT;
    if (t == "TIMER_ON") return BlockType::TIMER_ON;
    if (t == "TIMER_OFF") return BlockType::TIMER_OFF;
    if (t == "LATCH_SET") return BlockType::LATCH_SET;
    if (t == "LATCH_RESET") return BlockType::LATCH_RESET;
    if (t == "CONST_TRUE") return BlockType::CONST_TRUE;
    if (t == "CONST_FALSE") return BlockType::CONST_FALSE;
    return BlockType::CONST_FALSE;
}

const char* blockTypeToString(BlockType t) {
    switch (t) {
        case BlockType::CONTACT_NO: return "CONTACT_NO";
        case BlockType::CONTACT_NC: return "CONTACT_NC";
        case BlockType::COIL: return "COIL";
        case BlockType::AND: return "AND";
        case BlockType::OR: return "OR";
        case BlockType::NOT: return "NOT";
        case BlockType::TIMER_ON: return "TIMER_ON";
        case BlockType::TIMER_OFF: return "TIMER_OFF";
        case BlockType::LATCH_SET: return "LATCH_SET";
        case BlockType::LATCH_RESET: return "LATCH_RESET";
        case BlockType::CONST_TRUE: return "CONST_TRUE";
        case BlockType::CONST_FALSE: return "CONST_FALSE";
    }
    return "CONST_FALSE";
}

} // namespace

LadderEngine::LadderEngine() : blockCount(0), cycleMs(20) {
    memset(runtime, 0, sizeof(runtime));
}

void LadderEngine::init() {
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Falha ao montar LittleFS");
    }
    // Garantir arquivo de programa padrão na primeira inicialização
    if (!LittleFS.exists(PROGRAM_PATH)) {
        StaticJsonDocument<512> doc;
        doc["cycle_ms"] = cycleMs;
        JsonArray blocks = doc.createNestedArray("blocks");
        {
            JsonObject b0 = blocks.createNestedObject();
            b0["id"] = 0;
            b0["type"] = "CONTACT_NO";
            b0["io"] = 0; // IN0
        }
        {
            JsonObject b1 = blocks.createNestedObject();
            b1["id"] = 1;
            b1["type"] = "TIMER_ON";
            b1["a"] = 0;
            b1["delay_ms"] = 500;
        }
        {
            JsonObject b2 = blocks.createNestedObject();
            b2["id"] = 2;
            b2["type"] = "COIL";
            b2["a"] = 1;
            b2["io"] = 0; // OUT0
        }
        File f = LittleFS.open(PROGRAM_PATH, "w");
        if (f) {
            serializeJson(doc, f);
            f.close();
            Serial.println("[FS] Programa padrão criado");
        } else {
            Serial.println("[FS] Falha ao criar programa padrão");
        }
    }
    loadFromStorage();
}

void LadderEngine::clearProgram() {
    blockCount = 0;
    memset(runtime, 0, sizeof(runtime));
}

bool LadderEngine::loadFromStorage() {
    if (!LittleFS.exists(PROGRAM_PATH)) {
        Serial.println("[FS] Nenhum programa salvo");
        return false;
    }

    File f = LittleFS.open(PROGRAM_PATH, "r");
    if (!f) {
        Serial.println("[FS] Falha ao abrir programa");
        return false;
    }

    bool ok = loadProgramInternal(f);
    f.close();
    return ok;
}

bool LadderEngine::eraseStorage() {
    if (LittleFS.exists(PROGRAM_PATH)) {
        LittleFS.remove(PROGRAM_PATH);
    }
    clearProgram();
    return true;
}

bool LadderEngine::loadFromJson(const String& json, bool persist) {
    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("[JSON] Erro ao parsear: %s\n", err.c_str());
        return false;
    }

    bool parsed = parseDocument(doc);
    if (parsed && persist) {
        saveProgramInternal(json);
    }
    return parsed;
}

bool LadderEngine::parseDocument(const JsonDocument& doc) {
    if (!doc.containsKey("blocks")) {
        Serial.println("[JSON] Blocos ausentes");
        return false;
    }

    cycleMs = constrain(doc["cycle_ms"] | 20, 10, 100);

    JsonArrayConst arr = doc["blocks"].as<JsonArrayConst>();
    blockCount = arr.size() < MAX_BLOCKS ? arr.size() : MAX_BLOCKS;
    memset(runtime, 0, sizeof(runtime));

    for (uint8_t i = 0; i < blockCount; i++) {
        JsonObjectConst o = arr[i];
        BlockDef& b = blocks[i];
        String typeStr = o["type"].as<String>();
        b.type = blockTypeFromString(typeStr);
        b.inA = o["a"] | -1;
        b.inB = o["b"] | -1;
        b.ioIndex = o["io"] | 0;
        b.delayMs = o["delay_ms"] | 0;
    }
    return true;
}

bool LadderEngine::loadProgramInternal(File& f) {
    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, f);
    if (err) {
        Serial.printf("[JSON] Erro de leitura: %s\n", err.c_str());
        return false;
    }
    return parseDocument(doc);
}

bool LadderEngine::saveProgramInternal(const String& json) {
    File f = LittleFS.open(PROGRAM_PATH, "w");
    if (!f) {
        Serial.println("[FS] Falha ao salvar programa");
        return false;
    }
    f.print(json);
    f.close();
    return true;
}

String LadderEngine::serializeProgram() const {
    DynamicJsonDocument doc(8192);
    doc["cycle_ms"] = cycleMs;
    JsonArray arr = doc.createNestedArray("blocks");
    for (uint8_t i = 0; i < blockCount; i++) {
        JsonObject o = arr.createNestedObject();
        o["id"] = i;
        o["type"] = blockTypeToString(blocks[i].type);
        o["a"] = blocks[i].inA;
        o["b"] = blocks[i].inB;
        o["io"] = blocks[i].ioIndex;
        o["delay_ms"] = blocks[i].delayMs;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

void LadderEngine::setCycleMs(uint16_t periodMs) {
    cycleMs = constrain(periodMs, 10, 100);
}

bool LadderEngine::computeBlock(uint8_t idx, uint32_t now) {
    BlockDef& b = blocks[idx];
    BlockRuntime& r = runtime[idx];

    auto source = [&](int8_t id) {
        if (id < 0 || id >= blockCount) return false;
        return runtime[id].value;
    };

    bool inA = source(b.inA);
    bool inB = source(b.inB);
    bool val = false;

    switch (b.type) {
        case BlockType::CONTACT_NO:
            val = ioManager.getInput(b.ioIndex);
            break;
        case BlockType::CONTACT_NC:
            val = !ioManager.getInput(b.ioIndex);
            break;
        case BlockType::CONST_TRUE:
            val = true;
            break;
        case BlockType::CONST_FALSE:
            val = false;
            break;
        case BlockType::AND:
            val = inA && inB;
            break;
        case BlockType::OR:
            val = inA || inB;
            break;
        case BlockType::NOT:
            val = !inA;
            break;
        case BlockType::TIMER_ON:
            if (inA) {
                if (!r.timerActive) {
                    r.timerActive = true;
                    r.timerStart = now;
                }
                val = (now - r.timerStart) >= b.delayMs;
            } else {
                r.timerActive = false;
                val = false;
            }
            break;
        case BlockType::TIMER_OFF:
            if (inA) {
                r.timerActive = false;
                val = true;
            } else {
                if (!r.timerActive) {
                    r.timerActive = true;
                    r.timerStart = now;
                }
                val = (now - r.timerStart) < b.delayMs;
            }
            break;
        case BlockType::LATCH_SET:
            if (inA) {
                r.latched = true;
                ioManager.setOutput(b.ioIndex, true);
            }
            val = ioManager.getOutput(b.ioIndex);
            break;
        case BlockType::LATCH_RESET:
            if (inA) {
                r.latched = false;
                ioManager.setOutput(b.ioIndex, false);
            }
            val = ioManager.getOutput(b.ioIndex);
            break;
        case BlockType::COIL:
            val = inA;
            ioManager.setOutput(b.ioIndex, val);
            break;
    }

    runtime[idx].value = val;
    return val;
}

void LadderEngine::tick() {
    uint32_t now = millis();
    ioManager.scanInputs();

    for (uint8_t i = 0; i < blockCount; i++) {
        computeBlock(i, now);
    }

    ioManager.updateOutputs();
}
