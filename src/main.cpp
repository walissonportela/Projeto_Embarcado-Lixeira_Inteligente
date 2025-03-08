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

// ============================
// Definições de Hardware
// ============================

// Definições da tela OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensores
Adafruit_MPU6050 mpu;
DHTesp dht;  
Servo servoMotor;
Adafruit_BMP280 bmp(5);  // BMP280 conectado via SPI (CS = 5)

// Definição dos Pinos
#define MQ2_PIN 35       // Sensor de gás
#define POT_PIN 34       // Potenciômetro
#define HC_SR04_TRIG 0   // Trigger do Ultrassônico
#define HC_SR04_ECHO 2   // Echo do Ultrassônico
#define PIR_PIN 27       // Sensor PIR (movimento)
#define SERVO_PIN 4      // Servo Motor
#define BUZZER_PIN 16    // Buzzer

// LEDs Separados
#define LED_RED 32
#define LED_YELLOW 33
#define LED_GREEN 25

// Controle de Tempo
unsigned long lastDHTRead = 0;
bool tampaAberta = false;

#define ALTURA_LIXEIRA 400  // Altura total da lixeira em cm

// ============================
// Protótipos das Funções
// ============================
void controlarTampa();
void controlarLedsPeso(float weight);
void ligarLED(int ledPin);
void desligarLED(int ledPin);
float readGasPPM();
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);
void triggerAlarm(String message);
float readUltrasonic();
float calcularPorcentagemLixeira();
void readDHTSensor(float &humidity);

// ============================
// Configuração Inicial (Setup)
// ============================
void setup() {
    Serial.begin(115200);

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

    // Inicialização de sensores e atuadores
    Wire.begin();
    mpu.begin();
    bmp.begin();
    dht.setup(14, DHTesp::DHT22);
    servoMotor.attach(SERVO_PIN, 500, 2400);
    servoMotor.write(0);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("Erro ao iniciar OLED");
        while (1);
    }

    display.clearDisplay();
}

// ============================
// Loop Principal
// ============================
void loop() {
    float humidity;
    readDHTSensor(humidity);
    float temperatureBMP = bmp.readTemperature();
    float gasLevel = readGasPPM();
    float weight = map(analogRead(POT_PIN), 0, 4095, 0, 1000);
    float porcentagemLixo = calcularPorcentagemLixeira(); 

    // Controle da Tampa
    controlarTampa();

    // Alertas
    if (weight > 900) triggerAlarm("Peso maximo atingido!");
    if (porcentagemLixo > 95.0) triggerAlarm("Lixeira cheia!");
    if (temperatureBMP > 50.0) triggerAlarm("Perigo de incendio!");
    if (humidity > 80.0) triggerAlarm("Perigo de infiltracao!");

    // Controle dos LEDs
    controlarLedsPeso(weight);

    // Exibição no OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("Temperatura: "); display.print(temperatureBMP); display.print(" C");
    display.setCursor(0, 15);
    display.print("Umidade: "); display.print(humidity); display.print("%");
    display.setCursor(0, 25);
    display.print("Gases: "); display.print(gasLevel); display.print(" ppm");
    display.setCursor(0, 35);
    display.print("Peso: "); display.print(weight); display.print(" Kg");
    display.setCursor(0, 45);
    display.print("Capacidade: "); display.print(porcentagemLixo); display.print("%");
    display.display();
    
    delay(1000);
}

// ============================
// Funções Auxiliares
// ============================

void controlarTampa() {
    if (digitalRead(PIR_PIN) == HIGH) {
        servoMotor.write(0);
        delay(3000);
        servoMotor.write(90);
    }
}

void controlarLedsPeso(float weight) {
    desligarLED(LED_RED);
    desligarLED(LED_YELLOW);
    desligarLED(LED_GREEN);

    if (weight > 600) ligarLED(LED_RED);
    else if (weight > 300) ligarLED(LED_YELLOW);
    else ligarLED(LED_GREEN);
}

void ligarLED(int ledPin) { digitalWrite(ledPin, HIGH); }
void desligarLED(int ledPin) { digitalWrite(ledPin, LOW); }

float readGasPPM() { return analogRead(MQ2_PIN); }

float readUltrasonic() {
    digitalWrite(HC_SR04_TRIG, LOW);
    delayMicroseconds(5);
    digitalWrite(HC_SR04_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(HC_SR04_TRIG, LOW);
    return (pulseIn(HC_SR04_ECHO, HIGH, 30000) * 0.0343) / 2;
}

float calcularPorcentagemLixeira() {
    float distancia = readUltrasonic();  
    return fmax(0.0, fmin(((ALTURA_LIXEIRA - distancia) / ALTURA_LIXEIRA) * 100.0, 100.0));
}

void triggerAlarm(String message) {
    Serial.println(message);
    desligarLED(LED_YELLOW);
    desligarLED(LED_GREEN);
    for (int i = 0; i < 5; i++) {
        ligarLED(LED_RED);
        tone(BUZZER_PIN, 1000);
        delay(500);
        desligarLED(LED_RED);
        noTone(BUZZER_PIN);
        delay(500);
    }
}

void readDHTSensor(float &humidity) {
    if (millis() - lastDHTRead >= 2000) {
        lastDHTRead = millis();
        humidity = dht.getHumidity();
        if (isnan(humidity)) humidity = 0;
    }
}
