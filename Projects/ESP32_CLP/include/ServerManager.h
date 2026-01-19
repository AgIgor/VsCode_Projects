#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

class ServerManager {
public:
    ServerManager();
    void init(const char* staSsid = nullptr, const char* staPass = nullptr, bool forceAp = false);
    void begin();
    void handleClient();

private:
    WebServer server;
    bool apMode;
    String currentSsid;
    IPAddress currentIp;
    void setupRoutes();
};

extern ServerManager serverManager;
