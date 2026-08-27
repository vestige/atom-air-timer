#include <Arduino.h>
#include <Wire.h>
#include <M5Unified.h>
#include <Adafruit_NeoPixel.h>
#include <M5UnitENV.h>

#define LED_PIN 27
#define NUM_LEDS 25

#define SDA_PIN 26
#define SCL_PIN 32

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

SHT3X sht3x;
QMP6988 qmp;


// =============================
// タイマー
// =============================

const unsigned long TIMER_DURATION =
    60UL * 60UL * 1000UL;

// テスト時はこちら
// const unsigned long TIMER_DURATION = 10UL * 1000UL;

bool timerRunning = false;
bool timerFinished = false;

unsigned long timerStartedAt = 0;


// =============================
// ENV-III
// =============================

float currentTemperature = 0.0f;
float currentHumidity = 0.0f;

unsigned long lastEnvMeasureAt = 0;

const unsigned long ENV_INTERVAL = 2000;


// =============================
// 快適度
// =============================

enum ComfortLevel {
  COMFORTABLE,
  WARNING,
  UNCOMFORTABLE
};

ComfortLevel comfortLevel = COMFORTABLE;


// とりあえずの閾値
ComfortLevel getComfortLevel(
    float temperature,
    float humidity) {

  // かなり不快
  if (temperature >= 30.0f ||
      temperature <= 15.0f ||
      humidity >= 75.0f ||
      humidity <= 25.0f) {

    return UNCOMFORTABLE;
  }

  // 少し不快
  if (temperature >= 28.0f ||
      temperature <= 18.0f ||
      humidity >= 65.0f ||
      humidity <= 35.0f) {

    return WARNING;
  }

  return COMFORTABLE;
}


// =============================
// LED
// =============================

void setLeds(
    int count,
    uint32_t color,
    uint8_t brightness) {

  strip.setBrightness(brightness);
  strip.clear();

  for (int i = 0; i < count; i++) {
    strip.setPixelColor(i, color);
  }

  strip.show();
}


// タイマー終了表示
void showFinished(bool visible) {

  if (visible) {
    setLeds(
        NUM_LEDS,
        strip.Color(255, 0, 0),
        48);
  } else {
    strip.clear();
    strip.show();
  }
}


// 通常時の表示
void showNormalState(
    int ledCount,
    bool blinkVisible,
    bool timerActive) {

  uint8_t brightness = 8;

  switch (comfortLevel) {

    case COMFORTABLE:
      brightness = 8;
      break;

    case WARNING:
      brightness = 16;
      break;

    case UNCOMFORTABLE:

      if (!blinkVisible) {
        strip.clear();

        // 不快状態で全体が消えている間でも
        // タイマーランプは表示する
        if (timerActive) {
          strip.setBrightness(16);
          strip.setPixelColor(
              0,
              strip.Color(255, 255, 255));
        }

        strip.show();
        return;
      }

      brightness = 24;
      break;
  }

  strip.setBrightness(brightness);
  strip.clear();

  // タイマー動作中なら
  // LED 1〜24を残り時間表示に使う
  int startLed = timerActive ? 1 : 0;

  for (int i = 0; i < ledCount; i++) {
    int ledIndex = startLed + i;

    if (ledIndex >= NUM_LEDS) {
      break;
    }

    strip.setPixelColor(
        ledIndex,
        strip.Color(0, 0, 255));
  }

  // 左上：タイマー動作ランプ
  if (timerActive) {

    // 約0.5秒周期で点滅
    bool timerBlink =
        ((millis() / 500) % 2) == 0;

    if (timerBlink) {
      strip.setPixelColor(
          0,
          strip.Color(255, 255, 255));
    }
  }

  strip.show();
}


// =============================
// setup
// =============================

void setup() {

  M5.begin();

  Serial.begin(115200);
  delay(500);

  Serial.println("ATOM Comfort Timer started");


  // LED
  strip.begin();
  strip.setBrightness(24);
  strip.clear();
  strip.show();


  // ENV-III
  if (!qmp.begin(
          &Wire,
          QMP6988_SLAVE_ADDRESS_L,
          SDA_PIN,
          SCL_PIN,
          400000U)) {

    Serial.println("QMP6988 not found!");
  } else {
    Serial.println("QMP6988 found!");
  }


  if (!sht3x.begin(
          &Wire,
          SHT3X_I2C_ADDR,
          SDA_PIN,
          SCL_PIN,
          400000U)) {

    Serial.println("SHT30 not found!");

    while (1) {
      delay(100);
    }

  } else {
    Serial.println("SHT30 found!");
  }
}


// =============================
// loop
// =============================

void loop() {

  M5.update();

  unsigned long now = millis();


  // -----------------------------
  // ENV-III測定
  // -----------------------------

  if (now - lastEnvMeasureAt
      >= ENV_INTERVAL) {

    lastEnvMeasureAt = now;

    if (sht3x.update()) {

      currentTemperature =
          sht3x.cTemp;

      currentHumidity =
          sht3x.humidity;

      comfortLevel =
          getComfortLevel(
              currentTemperature,
              currentHumidity);


      Serial.print("Temperature: ");
      Serial.print(currentTemperature);
      Serial.print(" C");

      Serial.print("  Humidity: ");
      Serial.print(currentHumidity);
      Serial.print(" %");

      Serial.print("  Comfort: ");

      switch (comfortLevel) {

        case COMFORTABLE:
          Serial.println("COMFORTABLE");
          break;

        case WARNING:
          Serial.println("WARNING");
          break;

        case UNCOMFORTABLE:
          Serial.println("UNCOMFORTABLE");
          break;
      }
    }
  }


  // -----------------------------
  // ボタン
  // -----------------------------

  if (M5.BtnA.wasPressed()) {

    timerStartedAt = now;

    timerRunning = true;
    timerFinished = false;

    Serial.println("Timer started / reset");
  }


  // -----------------------------
  // 点滅タイミング
  // -----------------------------

  bool blinkVisible =
      ((now / 700) % 2) == 0;


  // -----------------------------
  // タイマー終了状態
  // -----------------------------

  if (timerFinished) {

    showFinished(blinkVisible);

    delay(10);
    return;
  }


  // -----------------------------
  // タイマー動作中
  // -----------------------------

  if (timerRunning) {

    unsigned long elapsed =
        now - timerStartedAt;

    if (elapsed >= TIMER_DURATION) {

      timerRunning = false;
      timerFinished = true;

      Serial.println("Timer finished!");

    } else {

      const int TIMER_LEDS = 24;

      int remaining =
          TIMER_LEDS -
          ((unsigned long)TIMER_LEDS *
          elapsed / TIMER_DURATION);

      showNormalState(
          remaining,
          blinkVisible,
          true);
      }

  } else {

    // -----------------------------
    // タイマー停止中
    //
    // ENVセンサーは動作し続ける
    // LEDは25個で環境状態を表示
    // -----------------------------

    showNormalState(
        NUM_LEDS,
        blinkVisible,
        false);
  }


  delay(10);
}
