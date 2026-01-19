#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "interface.h"

#define PIN_DOOR_OPEN       32
#define PIN_LOCK_SIGNAL     33
#define PIN_UNLOCK_SIGNAL   13
#define PIN_IGNITION        25
#define PIN_HEADLIGHT       26
#define PIN_DRL             27
#define PIN_INTERIOR_LIGHT  14
#define PIN_FOOT_LIGHT      12

const char* AP_SSID = "BCM_ESP32";
const char* AP_PASS = "12345678";

enum LightAction : uint8_t {
  ACTION_OFF = 0,
  ACTION_ON = 1
};

struct EventConfig {
  uint16_t duration;
  LightAction headlight;
  LightAction drl;
  LightAction interior;
  LightAction foot;
};

struct SystemConfig {
  EventConfig welcomeLight;
  EventConfig goodbyeLight;
  EventConfig followMeHome;
  EventConfig doorOpen;
  EventConfig ignitionOn;
  uint16_t maxOnTime;
};

SystemConfig config;
Preferences preferences;
WebServer server(80);

bool doorOpenState = false, doorOpenPrev = false;
bool lockState = false, lockPrev = false;
bool unlockState = false, unlockPrev = false;
bool ignitionState = false, ignitionPrev = false;

bool headlightOn = false, drlOn = false, interiorOn = false, footOn = false;

unsigned long welcomeTimer = 0, goodbyeTimer = 0, followMeTimer = 0, doorOpenTimer = 0;
bool welcomeActive = false, goodbyeActive = false, followMeActive = false, doorOpenActive = false;

#define MAX_LOG_ENTRIES 50
String eventLog[MAX_LOG_ENTRIES];
int logIndex = 0, logCount = 0;

void saveConfig();
void loadConfig();
String extractJsonValue(String json, String key);

void addLog(String msg) {
  eventLog[logIndex] = msg;
  logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
  if (logCount < MAX_LOG_ENTRIES) logCount++;
  Serial.println(msg);
}

void setLights(LightAction h, LightAction d, LightAction i, LightAction f) {
  if (h == ACTION_ON) headlightOn = true;
  else if (h == ACTION_OFF) headlightOn = false;
  
  if (d == ACTION_ON) drlOn = true;
  else if (d == ACTION_OFF) drlOn = false;
  
  if (i == ACTION_ON) interiorOn = true;
  else if (i == ACTION_OFF) interiorOn = false;
  
  if (f == ACTION_ON) footOn = true;
  else if (f == ACTION_OFF) footOn = false;
}

void applyOutputs() {
  digitalWrite(PIN_HEADLIGHT, headlightOn ? HIGH : LOW);
  digitalWrite(PIN_DRL, drlOn ? HIGH : LOW);
  digitalWrite(PIN_INTERIOR_LIGHT, interiorOn ? HIGH : LOW);
  digitalWrite(PIN_FOOT_LIGHT, footOn ? HIGH : LOW);
}

void readInputs() {
  doorOpenState = (digitalRead(PIN_DOOR_OPEN) == LOW);
  lockState = (digitalRead(PIN_LOCK_SIGNAL) == LOW);
  unlockState = (digitalRead(PIN_UNLOCK_SIGNAL) == LOW);
  ignitionState = (digitalRead(PIN_IGNITION) == LOW);
}

String extractJsonValue(String json, String key) {
  String searchKey = "\"" + key + "\":";
  int pos = json.indexOf(searchKey);
  if (pos == -1) return "0";
  
  int start = pos + searchKey.length();
  int end = json.indexOf(",", start);
  if (end == -1) end = json.indexOf("}", start);
  
  String value = json.substring(start, end);
  value.trim();
  return value;
}

void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleGetConfig() {
  String json = "{";
  json += "\"welcomeHeadlight\":" + String((int)config.welcomeLight.headlight) + ",";
  json += "\"welcomeDRL\":" + String((int)config.welcomeLight.drl) + ",";
  json += "\"welcomeInterior\":" + String((int)config.welcomeLight.interior) + ",";
  json += "\"welcomeFoot\":" + String((int)config.welcomeLight.foot) + ",";
  json += "\"welcomeDuration\":" + String(config.welcomeLight.duration) + ",";
  
  json += "\"goodbyeHeadlight\":" + String((int)config.goodbyeLight.headlight) + ",";
  json += "\"goodbyeDRL\":" + String((int)config.goodbyeLight.drl) + ",";
  json += "\"goodbyeInterior\":" + String((int)config.goodbyeLight.interior) + ",";
  json += "\"goodbyeFoot\":" + String((int)config.goodbyeLight.foot) + ",";
  json += "\"goodbyeDuration\":" + String(config.goodbyeLight.duration) + ",";
  
  json += "\"followMeHeadlight\":" + String((int)config.followMeHome.headlight) + ",";
  json += "\"followMeDRL\":" + String((int)config.followMeHome.drl) + ",";
  json += "\"followMeInterior\":" + String((int)config.followMeHome.interior) + ",";
  json += "\"followMeFoot\":" + String((int)config.followMeHome.foot) + ",";
  json += "\"followMeDuration\":" + String(config.followMeHome.duration) + ",";
  
  json += "\"doorOpenHeadlight\":" + String((int)config.doorOpen.headlight) + ",";
  json += "\"doorOpenDRL\":" + String((int)config.doorOpen.drl) + ",";
  json += "\"doorOpenInterior\":" + String((int)config.doorOpen.interior) + ",";
  json += "\"doorOpenFoot\":" + String((int)config.doorOpen.foot) + ",";
  json += "\"doorOpenDuration\":" + String(config.doorOpen.duration) + ",";
  
  json += "\"ignitionOnHeadlight\":" + String((int)config.ignitionOn.headlight) + ",";
  json += "\"ignitionOnDRL\":" + String((int)config.ignitionOn.drl) + ",";
  json += "\"ignitionOnInterior\":" + String((int)config.ignitionOn.interior) + ",";
  json += "\"ignitionOnFoot\":" + String((int)config.ignitionOn.foot) + ",";
  json += "\"ignitionOnDuration\":" + String(config.ignitionOn.duration);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleGetStatus() {
  String json = "{";
  json += "\"door\":" + String(doorOpenState ? "true" : "false") + ",";
  json += "\"lock\":" + String(lockState ? "true" : "false") + ",";
  json += "\"unlock\":" + String(unlockState ? "true" : "false") + ",";
  json += "\"ignition\":" + String(ignitionState ? "true" : "false") + ",";
  json += "\"headlight\":" + String(headlightOn ? "true" : "false") + ",";
  json += "\"drl\":" + String(drlOn ? "true" : "false") + ",";
  json += "\"interior\":" + String(interiorOn ? "true" : "false") + ",";
  json += "\"foot\":" + String(footOn ? "true" : "false") + ",";
  json += "\"logs\":[";
  
  for (int i = 0; i < logCount; i++) {
    json += "\"" + eventLog[(logIndex - logCount + i + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES] + "\"";
    if (i < logCount - 1) json += ",";
  }
  
  json += "]}";
  server.send(200, "application/json", json);
}

void handleSaveConfig() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    config.welcomeLight.headlight = (LightAction)extractJsonValue(body, "welcomeHeadlight").toInt();
    config.welcomeLight.drl = (LightAction)extractJsonValue(body, "welcomeDRL").toInt();
    config.welcomeLight.interior = (LightAction)extractJsonValue(body, "welcomeInterior").toInt();
    config.welcomeLight.foot = (LightAction)extractJsonValue(body, "welcomeFoot").toInt();
    config.welcomeLight.duration = extractJsonValue(body, "welcomeDuration").toInt();
    
    config.goodbyeLight.headlight = (LightAction)extractJsonValue(body, "goodbyeHeadlight").toInt();
    config.goodbyeLight.drl = (LightAction)extractJsonValue(body, "goodbyeDRL").toInt();
    config.goodbyeLight.interior = (LightAction)extractJsonValue(body, "goodbyeInterior").toInt();
    config.goodbyeLight.foot = (LightAction)extractJsonValue(body, "goodbyeFoot").toInt();
    config.goodbyeLight.duration = extractJsonValue(body, "goodbyeDuration").toInt();
    
    config.followMeHome.headlight = (LightAction)extractJsonValue(body, "followMeHeadlight").toInt();
    config.followMeHome.drl = (LightAction)extractJsonValue(body, "followMeDRL").toInt();
    config.followMeHome.interior = (LightAction)extractJsonValue(body, "followMeInterior").toInt();
    config.followMeHome.foot = (LightAction)extractJsonValue(body, "followMeFoot").toInt();
    config.followMeHome.duration = extractJsonValue(body, "followMeDuration").toInt();
    
    config.doorOpen.headlight = (LightAction)extractJsonValue(body, "doorOpenHeadlight").toInt();
    config.doorOpen.drl = (LightAction)extractJsonValue(body, "doorOpenDRL").toInt();
    config.doorOpen.interior = (LightAction)extractJsonValue(body, "doorOpenInterior").toInt();
    config.doorOpen.foot = (LightAction)extractJsonValue(body, "doorOpenFoot").toInt();
    config.doorOpen.duration = extractJsonValue(body, "doorOpenDuration").toInt();
    
    config.ignitionOn.headlight = (LightAction)extractJsonValue(body, "ignitionOnHeadlight").toInt();
    config.ignitionOn.drl = (LightAction)extractJsonValue(body, "ignitionOnDRL").toInt();
    config.ignitionOn.interior = (LightAction)extractJsonValue(body, "ignitionOnInterior").toInt();
    config.ignitionOn.foot = (LightAction)extractJsonValue(body, "ignitionOnFoot").toInt();
    config.ignitionOn.duration = extractJsonValue(body, "ignitionOnDuration").toInt();
    
    saveConfig();
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(400, "application/json", "{\"success\":false}");
  }
}

void loadConfig() {
  preferences.begin("bcm", false);
  
  config.welcomeLight.headlight = (LightAction)preferences.getUChar("wh", ACTION_ON);
  config.welcomeLight.drl = (LightAction)preferences.getUChar("wd", ACTION_ON);
  config.welcomeLight.interior = (LightAction)preferences.getUChar("wi", ACTION_OFF);
  config.welcomeLight.foot = (LightAction)preferences.getUChar("wf", ACTION_OFF);
  config.welcomeLight.duration = preferences.getUShort("wdur", 30);
  
  config.goodbyeLight.headlight = (LightAction)preferences.getUChar("gh", ACTION_ON);
  config.goodbyeLight.drl = (LightAction)preferences.getUChar("gd", ACTION_OFF);
  config.goodbyeLight.interior = (LightAction)preferences.getUChar("gi", ACTION_ON);
  config.goodbyeLight.foot = (LightAction)preferences.getUChar("gf", ACTION_ON);
  config.goodbyeLight.duration = preferences.getUShort("gdur", 30);
  
  config.followMeHome.headlight = (LightAction)preferences.getUChar("fh", ACTION_ON);
  config.followMeHome.drl = (LightAction)preferences.getUChar("fd", ACTION_OFF);
  config.followMeHome.interior = (LightAction)preferences.getUChar("fi", ACTION_ON);
  config.followMeHome.foot = (LightAction)preferences.getUChar("ff", ACTION_ON);
  config.followMeHome.duration = preferences.getUShort("fdur", 60);
  
  config.doorOpen.headlight = (LightAction)preferences.getUChar("dh", ACTION_OFF);
  config.doorOpen.drl = (LightAction)preferences.getUChar("dd", ACTION_OFF);
  config.doorOpen.interior = (LightAction)preferences.getUChar("di", ACTION_ON);
  config.doorOpen.foot = (LightAction)preferences.getUChar("df", ACTION_ON);
  config.doorOpen.duration = preferences.getUShort("ddur", 20);
  
  config.ignitionOn.headlight = (LightAction)preferences.getUChar("ih", ACTION_OFF);
  config.ignitionOn.drl = (LightAction)preferences.getUChar("id", ACTION_OFF);
  config.ignitionOn.interior = (LightAction)preferences.getUChar("ii", ACTION_OFF);
  config.ignitionOn.foot = (LightAction)preferences.getUChar("ifo", ACTION_OFF);
  config.ignitionOn.duration = preferences.getUShort("idur", 5);
  
  config.maxOnTime = preferences.getUShort("max", 600);
  
  preferences.end();
  addLog("Config loaded");
}

void saveConfig() {
  preferences.begin("bcm", false);
  
  preferences.putUChar("wh", (uint8_t)config.welcomeLight.headlight);
  preferences.putUChar("wd", (uint8_t)config.welcomeLight.drl);
  preferences.putUChar("wi", (uint8_t)config.welcomeLight.interior);
  preferences.putUChar("wf", (uint8_t)config.welcomeLight.foot);
  preferences.putUShort("wdur", config.welcomeLight.duration);
  
  preferences.putUChar("gh", (uint8_t)config.goodbyeLight.headlight);
  preferences.putUChar("gd", (uint8_t)config.goodbyeLight.drl);
  preferences.putUChar("gi", (uint8_t)config.goodbyeLight.interior);
  preferences.putUChar("gf", (uint8_t)config.goodbyeLight.foot);
  preferences.putUShort("gdur", config.goodbyeLight.duration);
  
  preferences.putUChar("fh", (uint8_t)config.followMeHome.headlight);
  preferences.putUChar("fd", (uint8_t)config.followMeHome.drl);
  preferences.putUChar("fi", (uint8_t)config.followMeHome.interior);
  preferences.putUChar("ff", (uint8_t)config.followMeHome.foot);
  preferences.putUShort("fdur", config.followMeHome.duration);
  
  preferences.putUChar("dh", (uint8_t)config.doorOpen.headlight);
  preferences.putUChar("dd", (uint8_t)config.doorOpen.drl);
  preferences.putUChar("di", (uint8_t)config.doorOpen.interior);
  preferences.putUChar("df", (uint8_t)config.doorOpen.foot);
  preferences.putUShort("ddur", config.doorOpen.duration);
  
  preferences.putUChar("ih", (uint8_t)config.ignitionOn.headlight);
  preferences.putUChar("id", (uint8_t)config.ignitionOn.drl);
  preferences.putUChar("ii", (uint8_t)config.ignitionOn.interior);
  preferences.putUChar("ifo", (uint8_t)config.ignitionOn.foot);
  preferences.putUShort("idur", config.ignitionOn.duration);
  
  preferences.putUShort("max", config.maxOnTime);
  
  preferences.end();
  addLog("Config saved");
}

void processEvents() {
  if (!unlockState && unlockPrev) {
    addLog(">>> UNLOCK (Welcome Light)");
    welcomeActive = true;
    welcomeTimer = millis();
    setLights(config.welcomeLight.headlight, config.welcomeLight.drl,
              config.welcomeLight.interior, config.welcomeLight.foot);
  }
  
  if (!lockState && lockPrev) {
    addLog(">>> LOCK (Follow Me Home)");
    followMeActive = true;
    followMeTimer = millis();
    setLights(config.followMeHome.headlight, config.followMeHome.drl,
              config.followMeHome.interior, config.followMeHome.foot);
  }
  
  if (!ignitionState && ignitionPrev) {
    addLog(">>> IGNITION ON");
    setLights(config.ignitionOn.headlight, config.ignitionOn.drl,
              config.ignitionOn.interior, config.ignitionOn.foot);
  }
  
  if (ignitionState && !ignitionPrev) {
    addLog(">>> IGNITION OFF (Goodbye Light)");
    goodbyeActive = true;
    goodbyeTimer = millis();
    setLights(config.goodbyeLight.headlight, config.goodbyeLight.drl,
              config.goodbyeLight.interior, config.goodbyeLight.foot);
  }
  
  if (!doorOpenState && doorOpenPrev) {
    addLog(">>> DOOR OPEN");
    doorOpenActive = true;
    doorOpenTimer = millis();
    setLights(config.doorOpen.headlight, config.doorOpen.drl,
              config.doorOpen.interior, config.doorOpen.foot);
  }
  
  if (doorOpenState && !doorOpenPrev) {
    addLog(">>> DOOR CLOSED");
    doorOpenActive = false;
  }
  
  doorOpenPrev = doorOpenState;
  lockPrev = lockState;
  unlockPrev = unlockState;
  ignitionPrev = ignitionState;
}

void updateTimers() {
  unsigned long now = millis();
  
  if (welcomeActive && (now - welcomeTimer) > (config.welcomeLight.duration * 1000UL)) {
    welcomeActive = false;
    addLog("Welcome OFF");
    headlightOn = false;
    drlOn = false;
    interiorOn = false;
    footOn = false;
  }
  
  if (goodbyeActive && (now - goodbyeTimer) > (config.goodbyeLight.duration * 1000UL)) {
    goodbyeActive = false;
    addLog("Goodbye OFF");
    headlightOn = false;
    drlOn = false;
    interiorOn = false;
    footOn = false;
  }
  
  if (followMeActive && (now - followMeTimer) > (config.followMeHome.duration * 1000UL)) {
    followMeActive = false;
    addLog("Follow Me OFF");
    headlightOn = false;
    drlOn = false;
    interiorOn = false;
    footOn = false;
  }
  
  if (doorOpenActive && (now - doorOpenTimer) > (config.doorOpen.duration * 1000UL)) {
    doorOpenActive = false;
    addLog("Door Light OFF");
    interiorOn = false;
    footOn = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(PIN_DOOR_OPEN, INPUT_PULLUP);
  pinMode(PIN_LOCK_SIGNAL, INPUT_PULLUP);
  pinMode(PIN_UNLOCK_SIGNAL, INPUT_PULLUP);
  pinMode(PIN_IGNITION, INPUT_PULLUP);
  
  pinMode(PIN_HEADLIGHT, OUTPUT);
  pinMode(PIN_DRL, OUTPUT);
  pinMode(PIN_INTERIOR_LIGHT, OUTPUT);
  pinMode(PIN_FOOT_LIGHT, OUTPUT);
  
  digitalWrite(PIN_HEADLIGHT, LOW);
  digitalWrite(PIN_DRL, LOW);
  digitalWrite(PIN_INTERIOR_LIGHT, LOW);
  digitalWrite(PIN_FOOT_LIGHT, LOW);
  
  loadConfig();
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  
  server.on("/", handleRoot);
  server.on("/getConfig", handleGetConfig);
  server.on("/getStatus", handleGetStatus);
  server.on("/saveConfig", HTTP_POST, handleSaveConfig);
  server.begin();
  
  addLog("System ready - " + WiFi.softAPIP().toString());
}

void loop() {
  server.handleClient();
  readInputs();
  processEvents();
  updateTimers();
  applyOutputs();
  delay(50);
}
