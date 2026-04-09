#include <Arduino.h>
#include <driver/twai.h>

static constexpr gpio_num_t CAN_RX_PIN = GPIO_NUM_4;
static constexpr gpio_num_t CAN_TX_PIN = GPIO_NUM_5;

static void printFrame(const twai_message_t &message) {
	Serial.print("ID=0x");
	if (message.extd) {
		Serial.print(message.identifier, HEX);
		Serial.print(" EXT ");
	} else {
		Serial.print(message.identifier, HEX);
		Serial.print(" STD ");
	}

	if (message.rtr) {
		Serial.print("RTR ");
	}

	Serial.print("DLC=");
	Serial.print(message.data_length_code);
	Serial.print(" DATA=");

	for (uint8_t i = 0; i < message.data_length_code; i++) {
		if (message.data[i] < 0x10) {
			Serial.print('0');
		}
		Serial.print(message.data[i], HEX);
		if (i < message.data_length_code - 1) {
			Serial.print(' ');
		}
	}

	Serial.println();
}

void setup() {
	Serial.begin(115200);
	delay(1000);

	twai_general_config_t generalConfig =
			TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
	twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_500KBITS();
	twai_filter_config_t filterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();

	esp_err_t result = twai_driver_install(&generalConfig, &timingConfig, &filterConfig);
	if (result != ESP_OK) {
		Serial.print("Erro ao instalar driver TWAI: ");
		Serial.println(result);
		while (true) {
			delay(1000);
		}
	}

	result = twai_start();
	if (result != ESP_OK) {
		Serial.print("Erro ao iniciar TWAI: ");
		Serial.println(result);
		while (true) {
			delay(1000);
		}
	}

	Serial.println("Sniffer CAN iniciado em RX=D4(GPIO4), TX=D5(GPIO5), 500 kbps");
}

void loop() {
	twai_message_t message;
	esp_err_t result = twai_receive(&message, pdMS_TO_TICKS(100));

	if (result == ESP_OK) {
		printFrame(message);
	}
}
