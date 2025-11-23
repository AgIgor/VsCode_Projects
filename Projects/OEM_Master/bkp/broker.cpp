#include <PicoMQTT.h>

const char* AP_SSID = "ESP32_MQTT";
const char* AP_PASS = "12345678";

class MQTT: public PicoMQTT::Server {
    protected:
        PicoMQTT::ConnectReturnCode auth(const char * client_id, const char * username, const char * password) override {
            // only accept client IDs which are 3 chars or longer
            if (String(client_id).length() < 3) {    // client_id is never NULL
                return PicoMQTT::CRC_IDENTIFIER_REJECTED;
            }

            // only accept connections if username and password are provided
            if (!username || !password) {  // username and password can be NULL
                // no username or password supplied
                return PicoMQTT::CRC_NOT_AUTHORIZED;
            }

            // accept two user/password combinations
            if (
                ((String(username) == "alice") && (String(password) == "secret"))
                || ((String(username) == "bob") && (String(password) == "password"))) {
                return PicoMQTT::CRC_ACCEPTED;
            }

            // reject all other credentials
            return PicoMQTT::CRC_BAD_USERNAME_OR_PASSWORD;
        }
} mqtt;

void setup() {
    // Setup serial
    Serial.begin(115200);

    // Connect to WiFi
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    Serial.println("📡 Wi-Fi AP inicializado!");
    Serial.print("➡ SSID: ");
    Serial.println(AP_SSID);
    Serial.print("➡ IP: ");
    Serial.println(WiFi.softAPIP());

    mqtt.begin();
}

void loop() {
    mqtt.loop();
    delay(10);
}
