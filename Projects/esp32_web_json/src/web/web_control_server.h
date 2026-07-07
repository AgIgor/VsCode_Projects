#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "vm/script_vm.h"

class WebControlServer {
public:
  explicit WebControlServer(ScriptVM& vm);

  void begin(const char* apSsid, const char* apPassword);
  void loop();

  IPAddress ip() const;

private:
  WebServer server_;
  ScriptVM& vm_;
  String script_;

  void setupRoutes();
  bool loadScriptFromStorage();
  bool saveScriptToStorage(const String& script);

  void handleRoot();
  void handleGetScript();
  void handleSetScript();
  void handleRun();
  void handleStop();
  void handleStatus();
};
