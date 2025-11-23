#include <PicoMQTT.h>

const char* WIFI_SSID = "ESP32_MQTT";
const char* WIFI_PASSWORD = "12345678";

PicoMQTT::Client mqtt(
    "192.168.4.1",    // broker address (or IP)
    1883,                   // broker port (defaults to 1883)
    "esp-client",           // Client ID
    "bob",             // MQTT username
    "password"              // MQTT password
);

// PicoMQTT::Client mqtt;       // This will work too, but configuration will have to be set later (e.g. in setup())

void setup() {
    // Setup serial
    Serial.begin(115200);
    pinMode(0, INPUT_PULLUP);

    // Connect to WiFi
    Serial.printf("Connecting to WiFi %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) { delay(1000); }
    Serial.println("WiFi connected.");

    // MQTT settings can be changed or set here instead
    mqtt.host = "192.168.4.1";
    mqtt.port = 1883;
    mqtt.client_id = "esp-" + WiFi.macAddress();
    mqtt.username = "bob";
    mqtt.password = "password";

    // Subscribe to a topic and attach a callback
    mqtt.subscribe("picomqtt/#", [](const char * topic, const char * payload) {
        // payload might be binary, but PicoMQTT guarantees that it's zero-terminated
        Serial.printf("Received message in topic '%s': %s\n", topic, payload);
    });

    mqtt.begin();
    delay(500);
    mqtt.publish("picomqtt/texto", "hello world!");
}

void loop() {
    mqtt.loop();
    delay(10);

    if(!digitalRead(0)){
        mqtt.publish("picomqtt/texto", String(millis()));
    }

    // Changing host, port, client_id, username or password here is OK too.  Changes will take effect after next
    // reconnect.  To force reconnection, explicitly disconnect the client by calling mqtt.disconnect().
}
