#include <Arduino.h>
#include "IOManager.h"
#include "LadderEngine.h"
#include "ServerManager.h"

uint32_t lastScan = 0;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("ESP32 CLP - inicializando");

    ioManager.init();
    ladderEngine.init();
    serverManager.init();
    serverManager.begin();
}

void loop() {
    serverManager.handleClient();

    uint32_t now = millis();
    if (now - lastScan >= ladderEngine.getCycleMs()) {
        ladderEngine.tick();
        lastScan = now;
    }
    delay(1); // cede CPU para Wi-Fi
}