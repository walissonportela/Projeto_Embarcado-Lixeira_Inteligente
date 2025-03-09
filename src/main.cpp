// =====================================
// Avaliação Final - Sistemas Embarcados
//
// Projeto: Lixeira Inteligente
//
// Nome: Joaquim Walisson Portela de Sousa
// Matrícula: 472152
//
// Link do Repositório: https://github.com/walissonportela/Projeto_Embarcado-Lixeira_Inteligente.git
// =====================================

// ============================
// 📌 Bibliotecas Usadas
// ============================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "DHTesp.h"
#include <ESP32Servo.h>
#include "esp32-hal-ledc.h"
#include <Adafruit_BMP280.h>
#include <SPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// ============================
// 📌 Definições de Hardware
// ============================
// 📺 Display OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 📌 Sensores
Adafruit_MPU6050 mpu;
DHTesp dht;
Adafruit_BMP280 bmp(5);

// 📌 Definição dos Pinos
#define MQ2_PIN 35       // Sensor de Gás
#define POT_PIN 34       // Potenciômetro simulando peso
#define HC_SR04_TRIG 0   // Sensor Ultrassônico - Trigger
#define HC_SR04_ECHO 2   // Sensor Ultrassônico - Echo
#define BUZZER_PIN 16    // Buzzer para alertas sonoros
#define LED_RED 32       // LED de Alerta (Vermelho)
#define LED_YELLOW 33    // LED de Atenção (Amarelo)
#define LED_GREEN 25     // LED de Status Normal (Verde)

#define ALTURA_LIXEIRA 400  // Altura da lixeira em milímetros

// ==================================================
// 📌 Estrutura para Armazenar Leituras dos Sensores
// ==================================================
typedef struct {
  float temperatureBMP = 0;
  float humidity = 0;
  float gasLevel = 0;
  float accelerometerX = 0;
  float accelerometerY = 0;
  float accelerometerZ = 0;
  float weight = 0;
  float capacity = 0;
} Sensores;

Sensores sensores;
sensors_event_t accelerometerEvent;

// ============================
// 📌 Controle de Sincronização
// ============================
SemaphoreHandle_t x_mutex = NULL;

// ============================
// 📌 Tasks dos Sensores
// ============================
void vTaskReadTemperature(void *pvParams);
void vTaskReadHumidity(void *pvParams);
void vTaskReadGas(void *pvParams);
void vTaskReadWeight(void *pvParams);
void vTaskReadCapacity(void *pvParams);
void vTaskReadAccelerometer(void *pvParams);
void vTaskUpdateOLED(void *pvParams);
void vTaskAlarm(void *pvParams);

// 📌 Handles das Tasks
TaskHandle_t handleAlarm;
TaskHandle_t handleUpdateOLED;
TaskHandle_t handleReadTemperature;
TaskHandle_t handleReadHumidity;
TaskHandle_t handleReadGas;
TaskHandle_t handleReadWeight;
TaskHandle_t handleReadCapacity;
TaskHandle_t handleReadAccelerometer;

// 📌 Funções Auxiliares
void controlarLedsPeso(float weight);
void ligarLED(int ledPin);
void desligarLED(int ledPin);
void triggerAlarm(String message);
float readUltrasonic();
float capacityPercent();
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);
void verificarProblema();

bool mostrandoAlerta = false; 
bool alarmTriggered = false;
String alarmMessage = "";

// ============================
// 📌 Setup - Inicialização do Sistema
// ============================
void setup() {
  Serial.begin(115200);

  // Inicialização do barramento I2C e do Display OLED
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println("⚠ Erro ao iniciar OLED");
      while (1);
  }
  Serial.println("✅ OLED iniciado");

  // ==============================
  // 📌 Tela Inicial
  // ==============================
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(20, 10);
  display.print("Inicializando...");
  display.display();
  vTaskDelay(pdMS_TO_TICKS(1500));

  // ==============================
  // 📌 Configuração dos Sensores
  // ==============================
  pinMode(MQ2_PIN, INPUT);
  pinMode(POT_PIN, INPUT);
  pinMode(HC_SR04_TRIG, OUTPUT);
  pinMode(HC_SR04_ECHO, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  mpu.begin();
  bmp.begin();
  dht.setup(14, DHTesp::DHT22);

  // Criar Mutex para sincronizar o OLED
  x_mutex = xSemaphoreCreateMutex();

  // ==============================
  // 📌 Criação das Tasks do FreeRTOS
  // ==============================
  if (x_mutex != NULL) {    
      xTaskCreatePinnedToCore(vTaskAlarm, "AlarmTask", 2048, NULL, 2, &handleAlarm, 1);
      xTaskCreatePinnedToCore(vTaskUpdateOLED, "UpdateOLED", 2048, NULL, 1, &handleUpdateOLED, 1);
      xTaskCreatePinnedToCore(vTaskReadTemperature, "ReadTemp", 2048, NULL, 1, &handleReadTemperature, 0);
      xTaskCreatePinnedToCore(vTaskReadHumidity, "ReadHum", 2048, NULL, 1, &handleReadHumidity, 0);
      xTaskCreatePinnedToCore(vTaskReadGas, "ReadGas", 2048, NULL, 1, &handleReadGas, 0);
      xTaskCreatePinnedToCore(vTaskReadWeight, "ReadWeight", 2048, NULL, 1, &handleReadWeight, 0);
      xTaskCreatePinnedToCore(vTaskReadCapacity, "ReadCapacity", 2048, NULL, 1, &handleReadCapacity, 0);
      xTaskCreatePinnedToCore(vTaskReadAccelerometer, "ReadAccel", 2048, NULL, 1, &handleReadAccelerometer, 0);
  }
  
}

void loop() {}

// ============================
// 📌 TASKS - LEITURAS DOS SENSORES
// ============================

/**
 * 📌 TASK: Atualiza o Display OLED.
 */
void vTaskUpdateOLED(void *pvParams) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  char buffer[20]; // Buffer para formatar os valores

  while (true) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);

      if (mostrandoAlerta) {
          // 🔴 Exibir alerta em vez das medições dos sensores
          if (xSemaphoreTake(x_mutex, portMAX_DELAY)) {

              display.setCursor(20, 10);
              display.print("!!! ALERTA !!!");

              display.setCursor(0, 30);
              display.print(alarmMessage);  // Exibe a mensagem do alarme
              display.display();
              xSemaphoreGive(x_mutex);
          }
      } else {
          // 🌡️ Exibir as medições dos sensores normalmente
          display.setCursor(0, 0);
          display.print("Temperatura:");
          sprintf(buffer, "%6.1f C", sensores.temperatureBMP);
          display.print(buffer);

          display.setCursor(0, 12);
          display.print("Umidade:");
          sprintf(buffer, "%6.1f %%", sensores.humidity);
          display.print(buffer);

          display.setCursor(0, 24);
          display.print("Gas:");
          sprintf(buffer, "%6.0f ppm", sensores.gasLevel);
          display.print(buffer);

          display.setCursor(0, 36);
          display.print("Peso:");
          sprintf(buffer, "%6.1f Kg", sensores.weight);
          display.print(buffer);

          display.setCursor(0, 48);
          display.print("Capacidade:");
          sprintf(buffer, "%6.1f %%", sensores.capacity);
          display.print(buffer);

          display.display();
      }

      vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1500));  // Atualiza o OLED a cada 1.5s
  }
}

/**
 * 📌 TASK: Lê a temperatura e ativa um alerta se estiver acima do limite.
 */
void vTaskReadTemperature(void *pvParams) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    sensores.temperatureBMP = mapFloat(bmp.readTemperature(), -123, 123, 0, 100);

    if (sensores.temperatureBMP > 50.0) {
      triggerAlarm("Perigo de incendio!");
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
  }
}

/**
 * 📌 TASK: Lê a umidade e verifica se há risco de infiltração.
 */
void vTaskReadHumidity(void *pvParams) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    sensores.humidity = dht.getHumidity();

    if (sensores.humidity > 80.0) {
      triggerAlarm("Risco de infiltracao!");
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
  }
}

/**
 * 📌 TASK: Lê o nível de gás e ativa alerta caso passe do limite.
 */
void vTaskReadGas(void *pvParams) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    sensores.gasLevel = mapFloat(analogRead(MQ2_PIN), 843, 4041, 0, 10000);

    if (sensores.gasLevel > 5000) {
      triggerAlarm("Nivel critico de gas!");
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1500));
  }
}

/**
 * 📌 TASK: Lê o peso da lixeira e controla os LEDs de status.
 */
void vTaskReadWeight(void *pvParams) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    sensores.weight = analogRead(POT_PIN);
    controlarLedsPeso(sensores.weight);

    if (sensores.weight > 3500) {

      triggerAlarm("Peso maximo atingido!");
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1500));
  }
}

/**
 * 📌 TASK: Lê a capacidade da lixeira e ativa alerta se estiver cheia.
 */
void vTaskReadCapacity(void *pvParams) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    sensores.capacity = capacityPercent();

    if (sensores.capacity > 95.0) {
      triggerAlarm("Lixeira cheia!");
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1500));
  }
}

/**
 * 📌 TASK: Monitora aceleração para identificar quedas da lixeira.
 */
void vTaskReadAccelerometer(void *pvParams) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    mpu.getAccelerometerSensor()->getEvent(&accelerometerEvent);
    sensores.accelerometerX = accelerometerEvent.acceleration.x;
    sensores.accelerometerY = accelerometerEvent.acceleration.y;
    sensores.accelerometerZ = accelerometerEvent.acceleration.z;

    if (abs(sensores.accelerometerX) > 5.0 || 
        abs(sensores.accelerometerY) > 5.0 || 
        abs(sensores.accelerometerZ) > 5.0) {
      triggerAlarm("Lixeira caiu!");
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
  }
}

/**
 * 📌 TASK: Ativa oa alarme para os níveis críticos de cada sensor.
 */
void vTaskAlarm(void *pvParams) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {

      if (alarmTriggered) {

          Serial.println(alarmMessage);

          // Desligar LEDS amarelo e verde
          desligarLED(LED_YELLOW);
          desligarLED(LED_GREEN);

          // Ativa o LED vermelho e o buzzer
          ligarLED(LED_RED);
          tone(BUZZER_PIN, 500);
          mostrandoAlerta = true;  

          vTaskDelay(pdMS_TO_TICKS(300)); 

          // Desliga o LED e o buzzer
          desligarLED(LED_RED);
          noTone(BUZZER_PIN);
          mostrandoAlerta = false;  

          vTaskDelay(pdMS_TO_TICKS(300));  

          verificarProblema();  // Verifica se o problema ainda persiste

      }

      vTaskDelay(pdMS_TO_TICKS(100));  
  }
}

// ============================
// 📌 FUNÇÕES AUXILIARES 
// ============================

/**
 * 📌 Ativa o alarme e define a mensagem de alerta.
 */
void triggerAlarm(String message) {
  alarmTriggered = true;
  alarmMessage = message;
}

/**
* 📌 Liga o LED indicado pelo pino.
*/
void ligarLED(int ledPin) {
  digitalWrite(ledPin, HIGH);
}

/**
* 📌 Desliga o LED indicado pelo pino.
*/
void desligarLED(int ledPin) {
  digitalWrite(ledPin, LOW);
}

/**
* 📌 Mede a distância usando o sensor ultrassônico.
* 💡 Retorna a distância em centímetros.
*/
float readUltrasonic() {
  digitalWrite(HC_SR04_TRIG, LOW);
  delayMicroseconds(5);
  digitalWrite(HC_SR04_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_SR04_TRIG, LOW);
  
  return (pulseIn(HC_SR04_ECHO, HIGH, 30000) * 0.0343) / 2;
}

/**
* 📌 Controla os LEDs da lixeira conforme o peso detectado.
*/
void controlarLedsPeso(float weight) {
  // Desliga todos os LEDs antes de definir o status correto
  desligarLED(LED_RED);
  desligarLED(LED_YELLOW);
  desligarLED(LED_GREEN);

  // Define o LED apropriado com base no peso
  if (weight > 3000) {
      ligarLED(LED_RED);      // 🔴 Peso crítico
  } else if (weight > 1500) {
      ligarLED(LED_YELLOW);   // 🟡 Peso médio
  } else {
      ligarLED(LED_GREEN);    // 🟢 Peso leve
  }
}

/**
* 📌 Calcula a porcentagem de capacidade da lixeira.
* 💡 Baseado na altura da lixeira e na distância do ultrassônico.
*/
float capacityPercent() {
  return fmax(0.0, fmin(((ALTURA_LIXEIRA - readUltrasonic()) / ALTURA_LIXEIRA) * 100.0, 100.0));
}

/**
* 📌 Mapeia um valor de um intervalo para outro.
* 💡 Calibrar valores dos sensores.
*/
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
* 📌 Verifica se as condições que acionaram o alarme ainda persistem.
* 💡 Se todos os valores estiverem dentro dos limites aceitáveis, o alarme é desativado.
*/
void verificarProblema() {
  if (sensores.temperatureBMP <= 50.0 &&
      sensores.humidity <= 80.0 &&
      sensores.gasLevel <= 5000 &&
      sensores.weight <= 3500 &&
      sensores.capacity <= 95.0 &&
      abs(sensores.accelerometerX) <= 5.0 &&
      abs(sensores.accelerometerY) <= 5.0 &&
      abs(sensores.accelerometerZ) <= 5.0) {
      alarmTriggered = false;  // 🔕 Desativa o alarme se todas as condições forem seguras.
  }
}
