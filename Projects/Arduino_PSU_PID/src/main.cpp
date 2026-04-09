#include <Arduino.h>
#include <EEPROM.h>

// ========== CONFIGURAÇÕES DE HARDWARE ==========
#define PWM_PIN 3              // Pino PWM para controle MOSFET (D3)
#define VOLTAGE_PIN A0         // Pino ADC para leitura de tensão
#define CURRENT_PIN A1         // Pino ADC para leitura de corrente
#define BAUD_RATE 115200       // Taxa de transmissão serial

// ========== CONSTANTES DE CALIBRAÇÃO ==========
// Divisor resistivo: 24V máximo mapeado para ~4.8V no ADC
// Usando R1=100k, R2=25k: Vout = Vin * (25k/(100k+25k)) = Vin * 0.2
// Fator de escala = 5.0 * 5 / 1023 ≈ 0.0245 V/LSB * fator divisor
#define VOLTAGE_DIVIDER 4.8   // 24V = 4096 ADC (ajuste conforme seu divisor)
#define VOLTAGE_SCALE (24.0 / 1023.0)  // Escala 24V para 10 bits

// Resistor shunt: 0.1Ω com ganho de amplificador (ex: TL072 ou similar)
// Para 3A máximo: Queda = 0.3V → com ganho 10 = 3V no ADC
#define SHUNT_RESISTOR 0.1    // Ohms
#define SHUNT_GAIN 10.0        // Ganho do amplificador (ajuste conforme seu circuito)
#define CURRENT_SCALE (5.0 / 1023.0 / SHUNT_GAIN / SHUNT_RESISTOR)  // Escala para Amperes

// ========== PARÂMETROS PID ==========
struct PIDConfig {
  float kp = 2.0;
  float ki = 0.5;
  float kd = 0.1;
  uint16_t sample_time_ms = 100;  // Tempo de amostragem em ms
};

// ========== ESTRUTURA DE ESTADO ==========
struct PSUState {
  float target_voltage = 12.0;
  float target_current = 0.5;
  float actual_voltage = 0.0;
  float actual_current = 0.0;
  float pwm_output = 0;
  uint8_t pwm_value = 0;
  bool enabled = false;
  bool tuning = false;
};

// ========== CLASSE PID CONTROLLER ==========
class PIDController {
private:
  float kp, ki, kd;
  float integral = 0.0;
  float last_error = 0.0;
  float output = 0.0;
  uint16_t sample_time_ms;
  unsigned long last_time = 0;
  float min_output = 0.0;
  float max_output = 255.0;

public:
  PIDController(float p, float i, float d, uint16_t sample_ms = 100) 
    : kp(p), ki(i), kd(d), sample_time_ms(sample_ms) {}

  void setGains(float p, float i, float d) {
    kp = p;
    ki = i;
    kd = d;
  }

  void getGains(float& p, float& i, float& d) {
    p = kp;
    i = ki;
    d = kd;
  }

  void setOutputLimits(float min_val, float max_val) {
    min_output = min_val;
    max_output = max_val;
  }

  void reset() {
    integral = 0.0;
    last_error = 0.0;
    output = 0.0;
    last_time = millis();
  }

  float calculate(float setpoint, float actual) {
    unsigned long now = millis();
    
    if (now - last_time < sample_time_ms) {
      return output;
    }

    float dt = (now - last_time) / 1000.0;  // Tempo em segundos
    last_time = now;

    float error = setpoint - actual;

    // Termo proporcional
    float p_term = kp * error;

    // Termo integral com anti-windup
    integral += error * dt;
    integral = constrain(integral, -100.0, 100.0);
    float i_term = ki * integral;

    // Termo derivativo
    float d_term = 0.0;
    if (dt > 0) {
      d_term = kd * (error - last_error) / dt;
    }
    last_error = error;

    // Saída PID
    output = p_term + i_term + d_term;
    output = constrain(output, min_output, max_output);

    return output;
  }
};

// ========== CLASSE PID AUTO-TUNING (ESTILO KLIPPER) ==========
class PIDAutoTuner {
private:
  float oscillation_amplitude = 0.0;
  float oscillation_period = 0.0;
  int measurement_count = 0;
  unsigned long tuning_start_time = 0;
  bool rising_detected = false;
  float prev_value = 0.0;
  float peak_max = 0.0;
  float peak_min = 999.0;
  unsigned long peak_start_time = 0;

public:
  void startTuning() {
    measurement_count = 0;
    tuning_start_time = millis();
    rising_detected = false;
    prev_value = 0.0;
    peak_max = 0.0;
    peak_min = 999.0;
    oscillation_amplitude = 0.0;
    oscillation_period = 0.0;
    peak_start_time = 0;
  }

  void addMeasurement(float value) {
    measurement_count++;

    // Detectar oscilação
    if (measurement_count > 5) {
      if (!rising_detected && value > prev_value) {
        rising_detected = true;
        peak_start_time = millis();
      } else if (rising_detected && value < prev_value) {
        oscillation_period = (millis() - peak_start_time) * 2;  // Período completo
        rising_detected = false;
      }

      peak_max = max(peak_max, value);
      peak_min = min(peak_min, value);
      oscillation_amplitude = (peak_max - peak_min) / 2.0;
    }

    prev_value = value;
  }

  bool isTuningComplete() {
    return measurement_count > 100 && oscillation_amplitude > 0;
  }

  void calculateGains(float& kp, float& ki, float& kd) {
    // Método de Ziegler-Nichols modificado
    float Ku = (4.0 * 255.0) / (M_PI * oscillation_amplitude);  // Ganho crítico
    float Tu = oscillation_period / 1000.0;  // Período em segundos

    // Ganhos para resposta rápida
    kp = 0.6 * Ku;
    ki = 1.2 * Ku / Tu;
    kd = 0.075 * Ku * Tu;

    // Limitar ganhos razoáveis
    kp = constrain(kp, 0.1, 5.0);
    ki = constrain(ki, 0.01, 1.0);
    kd = constrain(kd, 0.01, 0.5);
  }

  float getOscillationAmplitude() const { return oscillation_amplitude; }
  float getOscillationPeriod() const { return oscillation_period; }
};

// ========== VARIÁVEIS GLOBAIS ==========
PSUState psu;
PIDConfig pid_config;
PIDController voltage_controller(pid_config.kp, pid_config.ki, pid_config.kd, pid_config.sample_time_ms);
PIDController current_controller(1.0, 0.1, 0.05, pid_config.sample_time_ms);
PIDAutoTuner auto_tuner;
unsigned long last_read_time = 0;
unsigned long last_serial_time = 0;

// ========== FUNÇÕES DE LEITURA ==========
float readVoltage() {
  int raw = analogRead(VOLTAGE_PIN);
  return raw * VOLTAGE_SCALE;
}

float readCurrent() {
  int raw = analogRead(CURRENT_PIN);
  return raw * CURRENT_SCALE;
}

// ========== CONTROLE PWM ==========
void setPWMOutput(uint8_t value) {
  analogWrite(PWM_PIN, value);
}

// ========== PROCESSAMENTO SERIAL ==========
void handleSerialCommands() {
  if (Serial.available()) {
    char cmd = Serial.read();
    
    switch (cmd) {
      case 'V': {  // Definir tensão alvo
        float voltage = Serial.parseFloat();
        psu.target_voltage = constrain(voltage, 0.0, 24.0);
        Serial.print("SV:");
        Serial.println(psu.target_voltage, 2);
        break;
      }
      
      case 'C': {  // Definir corrente alvo
        float current = Serial.parseFloat();
        psu.target_current = constrain(current, 0.0, 3.0);
        Serial.print("SC:");
        Serial.println(psu.target_current, 2);
        break;
      }
      
      case 'E': {  // Enable/Disable
        int enable = Serial.parseInt();
        psu.enabled = (enable != 0);
        if (!psu.enabled) {
          setPWMOutput(0);
          voltage_controller.reset();
          current_controller.reset();
        }
        Serial.print("EN:");
        Serial.println(psu.enabled ? "ON" : "OFF");
        break;
      }
      
      case 'P': {  // Definir Kp
        pid_config.kp = Serial.parseFloat();
        voltage_controller.setGains(pid_config.kp, pid_config.ki, pid_config.kd);
        Serial.print("KP:");
        Serial.println(pid_config.kp, 4);
        break;
      }
      
      case 'I': {  // Definir Ki
        pid_config.ki = Serial.parseFloat();
        voltage_controller.setGains(pid_config.kp, pid_config.ki, pid_config.kd);
        Serial.print("KI:");
        Serial.println(pid_config.ki, 4);
        break;
      }
      
      case 'D': {  // Definir Kd
        pid_config.kd = Serial.parseFloat();
        voltage_controller.setGains(pid_config.kp, pid_config.ki, pid_config.kd);
        Serial.print("KD:");
        Serial.println(pid_config.kd, 4);
        break;
      }
      
      case 'T': {  // Iniciar auto-tuning
        psu.tuning = true;
        psu.enabled = true;
        psu.target_voltage = 12.0;  // Tensão de teste
        psu.pwm_output = 128;  // PWM para oscilação
        auto_tuner.startTuning();
        Serial.println("TUNE:START");
        break;
      }
      
      case 'G': {  // Get status
        Serial.print("V:");
        Serial.print(psu.actual_voltage, 2);
        Serial.print(",I:");
        Serial.print(psu.actual_current, 2);
        Serial.print(",TV:");
        Serial.print(psu.target_voltage, 2);
        Serial.print(",TC:");
        Serial.print(psu.target_current, 2);
        Serial.print(",PWM:");
        Serial.println(psu.pwm_value);
        break;
      }
      
      case 'H': {  // Help
        Serial.println("=== PSU PID Control ===");
        Serial.println("V<value> - Set target voltage (0-24V)");
        Serial.println("C<value> - Set target current (0-3A)");
        Serial.println("E<0|1>  - Enable(1)/Disable(0)");
        Serial.println("P<value> - Set Kp gain");
        Serial.println("I<value> - Set Ki gain");
        Serial.println("D<value> - Set Kd gain");
        Serial.println("T       - Start auto-tuning");
        Serial.println("G       - Get status");
        Serial.println("H       - Help");
        break;
      }
    }
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(BAUD_RATE);
  
  pinMode(PWM_PIN, OUTPUT);
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(CURRENT_PIN, INPUT);
  
  // Configurar PWM
  analogWrite(PWM_PIN, 0);
  
  // Inicializar PID
  voltage_controller.setGains(pid_config.kp, pid_config.ki, pid_config.kd);
  voltage_controller.setOutputLimits(0, 255);
  current_controller.setOutputLimits(0, 255);
  
  Serial.println("\n=== Arduino PSU PID Control Started ===");
  Serial.println("Type 'H' for help");
  Serial.flush();
  
  last_read_time = millis();
  last_serial_time = millis();
}

// ========== LOOP PRINCIPAL ==========
void loop() {
  unsigned long now = millis();
  
  // Leitura de sensores (a cada 100ms)
  if (now - last_read_time >= 100) {
    last_read_time = now;
    
    psu.actual_voltage = readVoltage();
    psu.actual_current = readCurrent();
    
    if (psu.enabled) {
      if (psu.tuning) {
        // Modo auto-tuning
        auto_tuner.addMeasurement(psu.actual_voltage);
        
        if (auto_tuner.isTuningComplete()) {
          float new_kp, new_ki, new_kd;
          auto_tuner.calculateGains(new_kp, new_ki, new_kd);
          
          pid_config.kp = new_kp;
          pid_config.ki = new_ki;
          pid_config.kd = new_kd;
          voltage_controller.setGains(new_kp, new_ki, new_kd);
          
          psu.tuning = false;
          psu.enabled = false;
          setPWMOutput(0);
          
          Serial.print("TUNE:COMPLETE KP=");
          Serial.print(new_kp, 4);
          Serial.print(" KI=");
          Serial.print(new_ki, 4);
          Serial.print(" KD=");
          Serial.println(new_kd, 4);
        } else {
          // Manter oscilação durante tuning
          psu.pwm_output = 128 + 50 * sin(2 * M_PI * now / 3000.0);
        }
      } else {
        // Controle PID normal - Controlar tensão
        psu.pwm_output = voltage_controller.calculate(psu.target_voltage, psu.actual_voltage);
      }
      
      psu.pwm_value = constrain((uint8_t)psu.pwm_output, 0, 255);
      setPWMOutput(psu.pwm_value);
    } else {
      psu.pwm_value = 0;
      setPWMOutput(0);
    }
  }
  
  // Processamento serial
  handleSerialCommands();
  
  // Enviar status periodicamente (a cada 500ms)
  if (now - last_serial_time >= 500) {
    last_serial_time = now;
    Serial.print(">V:");
    Serial.print(psu.actual_voltage, 2);
    Serial.print(" I:");
    Serial.print(psu.actual_current, 2);
    Serial.print(" P:");
    Serial.println(psu.pwm_value);
  }
}