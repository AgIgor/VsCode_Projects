#include <Arduino.h>

#include "counters/counter_store.h"
#include "vm/script_vm.h"
#include "web/web_control_server.h"

namespace {
constexpr const char* kApSsid = "ESP32-SCRIPT";
constexpr const char* kApPassword = "12345678";

CounterStore gCounters;
ScriptVM gVm(gCounters);
WebControlServer gWeb(gVm);
}  // namespace

void setup() {
	Serial.begin(115200);
	delay(300);

	gWeb.begin(kApSsid, kApPassword);

	Serial.println();
	Serial.println("======================================");
	Serial.println("ESP32 Script Runtime pronto");
	Serial.print("AP SSID: ");
	Serial.println(kApSsid);
	Serial.print("Acesse: http://");
	Serial.println(gWeb.ip());
	Serial.println("======================================");
}

void loop() {
	gWeb.loop();
	gVm.tick();
	delay(1);
}
