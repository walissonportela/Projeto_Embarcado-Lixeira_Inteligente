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

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// ============================
// Definições de Hardware
// ============================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensores
Adafruit_MPU6050 mpu;
DHTesp dht;  
Servo servoMotor;
Adafruit_BMP280 bmp(5);  

// Definição dos Pinos
#define MQ2_PIN 35      
#define POT_PIN 34      
#define HC_SR04_TRIG 0  
#define HC_SR04_ECHO 2  
#define PIR_PIN 27      
#define SERVO_PIN 4     
#define BUZZER_PIN 16   

// LEDs
#define LED_RED 32
#define LED_YELLOW 33
#define LED_GREEN 25

#define ALTURA_LIXEIRA 400  

// Leituras
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

// Handlers do FreeRTOS
SemaphoreHandle_t x_mutex = NULL;
TaskHandle_t task_OLED, task_SENSORS, task_ALERTS;

// ============================
// Protótipos das Funções
// ============================
void controlarTampa();
void controlarLedsPeso(float weight);
void ligarLED(int ledPin);
void desligarLED(int ledPin);
void triggerAlarm(String message);
float readUltrasonic();
float capacityPercent();
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);

void vTaskUpdateOLED(void *pvParams);
void vTaskReadSensors(void *pvParams);
void vTaskCheckAlerts(void *pvParams);

// ============================
// 📌 Configuração Inicial (Setup)
// ============================
void setup() {
  Serial.begin(115200);
  delay(500); 

  // Inicialização de sensores e atuadores
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println("⚠ Erro ao iniciar OLED");
      while (1);
  }
  Serial.println("✅ OLED iniciado com sucesso");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("Display OK!");
  display.display();
  delay(2000);

  // Configuração de pinos
  pinMode(MQ2_PIN, INPUT);
  pinMode(POT_PIN, INPUT);
  pinMode(HC_SR04_TRIG, OUTPUT);
  pinMode(HC_SR04_ECHO, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  // Inicialização de sensores
  mpu.begin();
  bmp.begin();
  dht.setup(14, DHTesp::DHT22);
  servoMotor.attach(SERVO_PIN, 500, 2400);
  servoMotor.write(90);

  // Cria o mutex para gerenciar o acesso às variáveis globais
  x_mutex = xSemaphoreCreateMutex();

  // Criação das Tasks no FreeRTOS
  if (x_mutex != NULL) {    
    xTaskCreatePinnedToCore(vTaskReadSensors, "task_sensors", 4096, NULL, 1, &task_SENSORS, 0);
    xTaskCreatePinnedToCore(vTaskCheckAlerts, "task_alerts", 4096, NULL, 1, &task_ALERTS, 0);

    xTaskCreatePinnedToCore(vTaskUpdateOLED, "task_oled", 4096, NULL, 1, &task_OLED, 1);
  }
}

void loop(){}

// ============================
// 📌 Funções com FreeRTOS
// ============================

void vTaskReadSensors(void *pvParams) {
  while (true) {
    if (xSemaphoreTake(x_mutex, portMAX_DELAY)) {
      sensores.temperatureBMP = bmp.readTemperature();
      sensores.humidity = dht.getHumidity();
      sensores.gasLevel = mapFloat(analogRead(MQ2_PIN), 843, 4041, 0, 50000);
      sensores.weight = map(analogRead(POT_PIN), 0, 4095, 0, 1000);
      sensores.capacity = capacityPercent();

      mpu.getAccelerometerSensor()->getEvent(&accelerometerEvent);
      sensores.accelerometerX = accelerometerEvent.acceleration.x;
      sensores.accelerometerY = accelerometerEvent.acceleration.y;
      sensores.accelerometerZ = accelerometerEvent.acceleration.z;

      controlarTampa();
      controlarLedsPeso(sensores.weight);

      xSemaphoreGive(x_mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void vTaskUpdateOLED(void *pvParams) {
  while (true) {
    if (xSemaphoreTake(x_mutex, portMAX_DELAY)) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Temp: "); display.print(sensores.temperatureBMP); display.print(" C");
      display.setCursor(0, 15);
      display.print("Umidade: "); display.print(sensores.humidity); display.print("%");
      display.setCursor(0, 25);
      display.print("Gases: "); display.print(sensores.gasLevel); display.print(" ppm");
      display.setCursor(0, 35);
      display.print("Peso: "); display.print(sensores.weight); display.print(" Kg");
      display.display();
      xSemaphoreGive(x_mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void vTaskCheckAlerts(void *pvParams) {
  while (true) {
      if (xSemaphoreTake(x_mutex, portMAX_DELAY)) {
          // Verifica se os valores são válidos antes de ativar o alarme
          if (!isnan(sensores.accelerometerX) && !isnan(sensores.accelerometerY) && !isnan(sensores.accelerometerZ)) {
              if (abs(sensores.accelerometerX) > 3 || abs(sensores.accelerometerY) > 3 || abs(sensores.accelerometerZ) > 3) {
                  triggerAlarm("Lixeira caiu!");
              }
          }

          if (!isnan(sensores.weight) && sensores.weight > 900) {
              triggerAlarm("Peso máximo atingido!");
          }

          if (!isnan(sensores.capacity) && sensores.capacity > 95.0) {
              triggerAlarm("Lixeira cheia!");
          }

          if (!isnan(sensores.temperatureBMP) && sensores.temperatureBMP > 50.0) {
              triggerAlarm("Perigo de incêndio!");
          }

          if (!isnan(sensores.humidity) && sensores.humidity > 80.0) {
              triggerAlarm("Perigo de infiltração!");
          }

          xSemaphoreGive(x_mutex);
      }
      vTaskDelay(pdMS_TO_TICKS(2000));  // Delay para evitar sobrecarga da CPU
  }
}


// ============================
// 📌 Funções Auxiliares
// ============================

void ligarLED(int ledPin) { digitalWrite(ledPin, HIGH); }
void desligarLED(int ledPin) { digitalWrite(ledPin, LOW); }

void triggerAlarm(String message) {
  Serial.println(message);
  desligarLED(LED_YELLOW);
  desligarLED(LED_GREEN);
  for (int i = 0; i < 5; i++) {
      ligarLED(LED_RED);
      tone(BUZZER_PIN, 500);
      vTaskDelay(pdMS_TO_TICKS(500));
      desligarLED(LED_RED);
      noTone(BUZZER_PIN);
      vTaskDelay(pdMS_TO_TICKS(500));
  }
}

float readUltrasonic() {
  digitalWrite(HC_SR04_TRIG, LOW);
  delayMicroseconds(5);
  digitalWrite(HC_SR04_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_SR04_TRIG, LOW);
  return (pulseIn(HC_SR04_ECHO, HIGH, 30000) * 0.0343) / 2;
}

// Função para controlar a tampa da lixeira com o PIR
void controlarTampa() {
  if (digitalRead(PIR_PIN) == HIGH) {
      Serial.println("Movimento detectado! Abrindo tampa...");
      for (int cont = 90; cont >= 0; cont--) { 
          delay(15);  
      }
      delay(3000); // Espera 3 segundos
      Serial.println("Fechando tampa...");
      for (int cont = 0; cont <= 90; cont++) {
          servoMotor.write(cont);
          delay(15);
      }
  }
}

// Função para ligar um LED e desligar os outros
void controlarLedsPeso(float weight) {
  desligarLED(LED_RED);
  desligarLED(LED_YELLOW);
  desligarLED(LED_GREEN);

  if (weight > 600) {
      ligarLED(LED_RED);
  } else if (weight > 300 && weight <= 600) {
      ligarLED(LED_YELLOW);
  } else if (weight >= 0 && weight < 300){
      ligarLED(LED_GREEN);
  }
}

float capacityPercent() {
  return fmax(0.0, fmin(((ALTURA_LIXEIRA - readUltrasonic()) / ALTURA_LIXEIRA) * 100.0, 100.0));
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

