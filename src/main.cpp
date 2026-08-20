#include <Arduino.h>
#include <Wire.h>
#include <M5Unified.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SGP30.h>

#define LED_PIN 27
#define NUM_LEDS 25

#define SDA_PIN 26
#define SCL_PIN 32

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_SGP30 sgp;

//const unsigned long TIMER_DURATION = 10UL * 1000UL;
const unsigned long TIMER_DURATION = 60UL * 60UL * 1000UL;

bool timerRunning = false;
bool timerFinished = false;
unsigned long timerStartedAt = 0;

uint16_t currentTvoc = 0;

unsigned long lastSensorMeasureAt = 0;

// SGP30は約1秒間隔で測定
const unsigned long SENSOR_INTERVAL = 1000;

// TVOC値からLED色を決める
uint32_t getTvocColor(uint16_t tvoc) {
  if (tvoc < 10) {
    // ほぼ変化なし：青
    return strip.Color(0, 0, 255);
  }

  if (tvoc < 30) {
    // 少し変化：青緑
    return strip.Color(0, 180, 120);
  }

  if (tvoc < 100) {
    // はっきり変化：黄色
    return strip.Color(255, 180, 0);
  }

  // 大きな変化：オレンジ
  return strip.Color(255, 60, 0);
}


// 残り時間をLED数で表示する
void showTimer(int remaining) {
  strip.clear();

  uint32_t color = getTvocColor(currentTvoc);

  for (int i = 0; i < remaining; i++) {
    strip.setPixelColor(i, color);
  }

  strip.show();
}


// タイマー終了
void showFinished() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(
      i,
      strip.Color(255, 0, 0)
    );
  }

  strip.show();
}


void setup() {
  M5.begin();

  Serial.begin(115200);
  delay(500);

  Serial.println("ATOM Timer + TVOC started");

  // LED
  strip.begin();
  strip.setBrightness(32);
  strip.clear();
  strip.show();

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // SGP30
  if (!sgp.begin()) {
    Serial.println("SGP30 sensor not found!");

    while (1) {
      delay(100);
    }
  }

  Serial.println("SGP30 found!");
}


void loop() {
  M5.update();

  unsigned long now = millis();


  // -----------------------------
  // TVOC測定
  // -----------------------------
  if (now - lastSensorMeasureAt >= SENSOR_INTERVAL) {

    lastSensorMeasureAt = now;

    if (sgp.IAQmeasure()) {
      currentTvoc = sgp.TVOC;

      Serial.print("TVOC: ");
      Serial.print(currentTvoc);
      Serial.print(" ppb");

      Serial.print("  eCO2: ");
      Serial.print(sgp.eCO2);
      Serial.println(" ppm");
    }
  }


  // -----------------------------
  // ボタン
  // -----------------------------
  if (M5.BtnA.wasPressed()) {

    timerStartedAt = now;
    timerRunning = true;
    timerFinished = false;

    showTimer(NUM_LEDS);

    Serial.println("Timer started / reset");
  }


  // -----------------------------
  // タイマー
  // -----------------------------
  if (timerRunning) {

    unsigned long elapsed =
        now - timerStartedAt;

    if (elapsed >= TIMER_DURATION) {

      timerRunning = false;
      timerFinished = true;

      showFinished();

      Serial.println("Timer finished!");

    } else {

      int remaining =
          NUM_LEDS -
          ((unsigned long)NUM_LEDS *
           elapsed / TIMER_DURATION);

      showTimer(remaining);
    }
  }


  delay(10);
}
