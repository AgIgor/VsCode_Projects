#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "web.h"
#include "ladder.h"

WebServer server(80);
Preferences prefs;

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleProgram() {
  String body = server.arg("plain");
  saveProgram(body);
  loadProgram();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void setup() {
  Serial.begin(115200);

  WiFi.softAP("ESP32_CLP", "12345678");

  server.on("/", handleRoot);
  server.on("/program", HTTP_POST, handleProgram);
  server.begin();

  prefs.begin("clp", false);
  loadProgram();

  initIO();
}

void loop() {
  server.handleClient();
  scanCycle();
}
