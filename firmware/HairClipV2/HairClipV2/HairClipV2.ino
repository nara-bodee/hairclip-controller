#include <TFT_eSPI.h>
#include <U8g2_for_TFT_eSPI.h>

#include <esp_sleep.h>
#include <Preferences.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <math.h>


// ======================================================
// VERSION
// ======================================================

#define FIRMWARE_VERSION "1.0.0"


// ======================================================
// PRODUCTION SETTINGS
// ======================================================

// true  = แสดง Serial debug
// false = Production mode
const bool DEBUG_MODE =
  false;


// IMPORTANT:
// Production ต้องเป็น false เสมอ
const bool LOW_BATTERY_TEST_MODE =
  false;


// ======================================================
// HARDWARE
// ======================================================

#define TFT_BL 4
#define POWER_BUTTON 35

#define BATTERY_ADC_EN 14
#define BATTERY_ADC_PIN 34


// ======================================================
// DISPLAY
// ======================================================

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite sprite =
  TFT_eSprite(&tft);

U8g2_for_TFT_eSPI u8f;


String message =
  "HELLO WORLD!";


int x = 0;
int textWidth = 0;


const int scrollSpeed =
  1;

const int frameDelay =
  25;

// ขนาดข้อความ
// 1 = ขนาดเดิม
// 2 = ใหญ่ 2 เท่า
const int TEXT_SCALE =
  2;


bool displayEnabled =
  true;

bool scrollEnabled =
  true;


// ======================================================
// ICON CONFIG
// ======================================================

const int ICON_WIDTH =
  30;

const int ICON_GAP =
  4;


// ======================================================
// POWER
// ======================================================

enum PowerSource {
  POWER_UNKNOWN,
  POWER_BATTERY,
  POWER_EXTERNAL
};


PowerSource powerSource =
  POWER_UNKNOWN;


float batteryVoltage =
  0.0f;


int batteryPercent =
  -1;


float powerSenseVoltage =
  0.0f;


unsigned long lastPowerRead =
  0;


// ESP32 วัดไฟทุก 5 วินาที
const unsigned long POWER_READ_INTERVAL =
  5000;


// จากการทดสอบจริง
// Battery ~3.x–4.2V
// USB ~4.6–4.7V
const float EXTERNAL_POWER_THRESHOLD =
  4.35f;


// ======================================================
// BATTERY SAFETY
// ======================================================

enum BatterySafetyState {
  BATTERY_SAFE,
  BATTERY_LOW,
  BATTERY_CRITICAL
};


BatterySafetyState batterySafetyState =
  BATTERY_SAFE;


// เริ่ม Low
const float LOW_BATTERY_VOLTAGE =
  3.60f;


// เริ่ม Critical
const float CRITICAL_BATTERY_VOLTAGE =
  3.50f;


// Shutdown protection
const float BATTERY_SHUTDOWN_VOLTAGE =
  3.45f;


// Test threshold
const float TEST_SHUTDOWN_VOLTAGE =
  4.15f;


// ต้องต่ำกว่า threshold
// ต่อเนื่อง 3 ครั้ง
const int LOW_BATTERY_CONFIRM_COUNT =
  3;


int lowBatteryConfirmCount =
  0;


// ======================================================
// RUNTIME DIAGNOSTICS
// ======================================================

bool runtimeActive =
  false;


unsigned long runtimeStartMs =
  0;


unsigned long runtimeFrozenSeconds =
  0;


int runtimeStartPercent =
  -1;


int runtimeStartMilliVolts =
  0;


// ======================================================
// STORAGE
// ======================================================

Preferences preferences;


// ======================================================
// BLE
// ======================================================

#define SERVICE_UUID \
  "c7a10001-6c9e-4d5d-a001-123456789abc"

#define CHARACTERISTIC_UUID \
  "c7a10002-6c9e-4d5d-a001-123456789abc"


SemaphoreHandle_t commandMutex;

SemaphoreHandle_t stateMutex;


String pendingCommand =
  "";


bool newCommandReady =
  false;


// ======================================================
// DEBUG HELPERS
// ======================================================

void debugLine(
  const String &text
) {

  if (
    !DEBUG_MODE
  ) {

    return;
  }


  Serial.println(
    text
  );
}


// ======================================================
// ICONS
// ======================================================

void drawHeart(
  int cx,
  int cy,
  uint16_t color
) {

  sprite.fillCircle(
    cx - 6,
    cy - 4,
    7,
    color
  );


  sprite.fillCircle(
    cx + 6,
    cy - 4,
    7,
    color
  );


  sprite.fillTriangle(
    cx - 13,
    cy - 2,

    cx + 13,
    cy - 2,

    cx,
    cy + 16,

    color
  );
}


// ------------------------------------------------------

void drawStar(
  int cx,
  int cy,
  uint16_t color
) {

  const int points =
    10;


  int px[points];

  int py[points];


  for (
    int i = 0;
    i < points;
    i++
  ) {

    float angle =
      -PI / 2
      +
      i * PI / 5;


    float radius =
      (
        i % 2 == 0
      )
        ? 15
        : 7;


    px[i] =
      cx
      +
      cos(angle)
      *
      radius;


    py[i] =
      cy
      +
      sin(angle)
      *
      radius;
  }


  for (
    int i = 0;
    i < points;
    i++
  ) {

    int next =
      (
        i + 1
      )
      %
      points;


    sprite.drawLine(
      px[i],
      py[i],

      px[next],
      py[next],

      color
    );
  }
}


// ------------------------------------------------------

void drawSmile(
  int cx,
  int cy,
  uint16_t color
) {

  sprite.drawCircle(
    cx,
    cy,
    15,
    color
  );


  sprite.fillCircle(
    cx - 5,
    cy - 4,
    2,
    color
  );


  sprite.fillCircle(
    cx + 5,
    cy - 4,
    2,
    color
  );


  sprite.drawLine(
    cx - 7,
    cy + 4,

    cx - 3,
    cy + 8,

    color
  );


  sprite.drawLine(
    cx - 3,
    cy + 8,

    cx + 3,
    cy + 8,

    color
  );


  sprite.drawLine(
    cx + 3,
    cy + 8,

    cx + 7,
    cy + 4,

    color
  );
}


// ------------------------------------------------------

void drawSun(
  int cx,
  int cy,
  uint16_t color
) {

  sprite.drawCircle(
    cx,
    cy,
    8,
    color
  );


  for (
    int i = 0;
    i < 8;
    i++
  ) {

    float angle =
      i * PI / 4;


    int x1 =
      cx
      +
      cos(angle) * 12;


    int y1 =
      cy
      +
      sin(angle) * 12;


    int x2 =
      cx
      +
      cos(angle) * 17;


    int y2 =
      cy
      +
      sin(angle) * 17;


    sprite.drawLine(
      x1,
      y1,

      x2,
      y2,

      color
    );
  }
}


// ------------------------------------------------------

void drawMoon(
  int cx,
  int cy,
  uint16_t color
) {

  sprite.fillCircle(
    cx,
    cy,
    15,
    color
  );


  sprite.fillCircle(
    cx + 7,
    cy - 4,
    14,
    TFT_BLACK
  );
}


// ------------------------------------------------------

void drawMusic(
  int cx,
  int cy,
  uint16_t color
) {

  sprite.fillCircle(
    cx - 6,
    cy + 10,
    4,
    color
  );


  sprite.fillCircle(
    cx + 9,
    cy + 6,
    4,
    color
  );


  sprite.drawLine(
    cx - 2,
    cy + 10,

    cx - 2,
    cy - 12,

    color
  );


  sprite.drawLine(
    cx + 13,
    cy + 6,

    cx + 13,
    cy - 16,

    color
  );


  sprite.drawLine(
    cx - 2,
    cy - 12,

    cx + 13,
    cy - 16,

    color
  );
}


// ======================================================
// ICON TOKENS
// ======================================================

bool matchIconToken(
  const char *p,
  const char *token
) {

  while (*token) {

    if (
      *p != *token
    ) {

      return false;
    }


    p++;
    token++;
  }


  return true;
}


// ------------------------------------------------------

int getIconTokenLength(
  const char *p
) {

  if (
    matchIconToken(
      p,
      ":heart:"
    )
  ) return 7;


  if (
    matchIconToken(
      p,
      ":star:"
    )
  ) return 6;


  if (
    matchIconToken(
      p,
      ":smile:"
    )
  ) return 7;


  if (
    matchIconToken(
      p,
      ":sun:"
    )
  ) return 5;


  if (
    matchIconToken(
      p,
      ":moon:"
    )
  ) return 6;


  if (
    matchIconToken(
      p,
      ":music:"
    )
  ) return 7;


  return 0;
}


// ------------------------------------------------------

void drawIconToken(
  const char *p,
  int cx,
  int cy
) {

  if (
    matchIconToken(
      p,
      ":heart:"
    )
  ) {

    drawHeart(
      cx,
      cy,
      TFT_WHITE
    );

    return;
  }


  if (
    matchIconToken(
      p,
      ":star:"
    )
  ) {

    drawStar(
      cx,
      cy,
      TFT_WHITE
    );

    return;
  }


  if (
    matchIconToken(
      p,
      ":smile:"
    )
  ) {

    drawSmile(
      cx,
      cy,
      TFT_WHITE
    );

    return;
  }


  if (
    matchIconToken(
      p,
      ":sun:"
    )
  ) {

    drawSun(
      cx,
      cy,
      TFT_WHITE
    );

    return;
  }


  if (
    matchIconToken(
      p,
      ":moon:"
    )
  ) {

    drawMoon(
      cx,
      cy,
      TFT_WHITE
    );

    return;
  }


  if (
    matchIconToken(
      p,
      ":music:"
    )
  ) {

    drawMusic(
      cx,
      cy,
      TFT_WHITE
    );

    return;
  }
}


// ======================================================
// THAI
// ======================================================

bool isThaiCombiningMark(
  uint16_t codepoint
) {

  if (
    codepoint ==
    0x0E31
  ) {

    return true;
  }


  if (
    codepoint >= 0x0E34
    &&
    codepoint <= 0x0E3A
  ) {

    return true;
  }


  if (
    codepoint >= 0x0E47
    &&
    codepoint <= 0x0E4E
  ) {

    return true;
  }


  return false;
}


// ======================================================
// UTF-8
// ======================================================

uint16_t nextUTF8(
  const char *&p
) {

  uint8_t c =
    (uint8_t)*p++;


  if (
    c < 0x80
  ) {

    return c;
  }


  if (
    (c & 0xE0)
    ==
    0xC0
  ) {

    uint16_t result =
      (c & 0x1F)
      << 6;


    result |=
      (
        (uint8_t)*p++
        &
        0x3F
      );


    return result;
  }


  if (
    (c & 0xF0)
    ==
    0xE0
  ) {

    uint16_t result =
      (c & 0x0F)
      << 12;


    result |=
      (
        (
          (uint8_t)*p++
          &
          0x3F
        )
        << 6
      );


    result |=
      (
        (uint8_t)*p++
        &
        0x3F
      );


    return result;
  }


  if (
    (c & 0xF8)
    ==
    0xF0
  ) {

    p += 3;

    return '?';
  }


  return '?';
}


// ------------------------------------------------------

void codepointToUTF8(
  uint16_t codepoint,
  char *buffer
) {

  buffer[0] = '\0';
  buffer[1] = '\0';
  buffer[2] = '\0';
  buffer[3] = '\0';


  if (
    codepoint < 0x80
  ) {

    buffer[0] =
      (char)codepoint;

    return;
  }


  if (
    codepoint < 0x800
  ) {

    buffer[0] =
      0xC0
      |
      (
        codepoint >> 6
      );


    buffer[1] =
      0x80
      |
      (
        codepoint
        &
        0x3F
      );


    return;
  }


  buffer[0] =
    0xE0
    |
    (
      codepoint >> 12
    );


  buffer[1] =
    0x80
    |
    (
      (
        codepoint >> 6
      )
      &
      0x3F
    );


  buffer[2] =
    0x80
    |
    (
      codepoint
      &
      0x3F
    );
}


// ======================================================
// TEXT WIDTH
// ======================================================

int getGlyphAdvance(
  uint16_t codepoint
) {

  char utf8[4];


  codepointToUTF8(
    codepoint,
    utf8
  );


  return u8f.getUTF8Width(
    utf8
  );
}


// ------------------------------------------------------

int measureRichText(
  const String &text
) {

  const char *p =
    text.c_str();


  int width =
    0;


  while (*p) {

    int iconLength =
      getIconTokenLength(
        p
      );


    if (
      iconLength > 0
    ) {

      width +=
        ICON_WIDTH
        +
        ICON_GAP;


      p +=
        iconLength;


      continue;
    }


    uint16_t codepoint =
      nextUTF8(
        p
      );


    if (
      isThaiCombiningMark(
        codepoint
      )
    ) {

      continue;
    }


    width +=
      getGlyphAdvance(
        codepoint
      );
  }


  return width;
}


// ======================================================
// DRAW RICH TEXT
// ======================================================

int drawRichText(
  int startX,
  int baselineY,
  const String &text
) {

  int cursorX =
    startX;


  int baseX =
    startX;


  const char *p =
    text.c_str();


  while (*p) {

    int iconLength =
      getIconTokenLength(
        p
      );


    if (
      iconLength > 0
    ) {

      int iconCenterX =
        cursorX
        +
        ICON_WIDTH / 2;


      int iconCenterY =
        baselineY
        -
        8;


      drawIconToken(
        p,
        iconCenterX,
        iconCenterY
      );


      cursorX +=
        ICON_WIDTH
        +
        ICON_GAP;


      baseX =
        cursorX;


      p +=
        iconLength;


      continue;
    }


    uint16_t codepoint =
      nextUTF8(
        p
      );


    if (
      isThaiCombiningMark(
        codepoint
      )
    ) {

      u8f.drawGlyph(
        baseX,
        baselineY,
        codepoint
      );


      continue;
    }


    baseX =
      cursorX;


    int advance =
      u8f.drawGlyph(
        cursorX,
        baselineY,
        codepoint
      );


    cursorX +=
      advance;
  }


  return
    cursorX
    -
    startX;
}


// ======================================================
// TEXT POSITION
// ======================================================

int getTextBaselineY() {

  int ascent =
    u8f.getFontAscent();


  int descent =
    u8f.getFontDescent();


  int fontHeight =
    ascent
    -
    descent;


  return
    (
      (
        sprite.height()
        -
        fontHeight
      )
      /
      2
    )
    +
    ascent;
}


// ======================================================
// ADC
// ======================================================

float readPowerSenseVoltage() {

  const int samples =
    20;


  uint32_t totalMilliVolts =
    0;


  for (
    int i = 0;
    i < samples;
    i++
  ) {

    totalMilliVolts +=
      analogReadMilliVolts(
        BATTERY_ADC_PIN
      );


    delay(
      5
    );
  }


  float averageMilliVolts =
    totalMilliVolts
    /
    (float)samples;


  return
    (
      averageMilliVolts
      *
      2.0f
    )
    /
    1000.0f;
}


// ======================================================
// BATTERY PERCENT
// ======================================================

int voltageToPercent(
  float voltage
) {

  const float voltages[] = {

    4.20,
    4.15,
    4.11,
    4.08,
    4.02,

    3.98,
    3.95,
    3.91,
    3.87,
    3.85,

    3.82,
    3.80,
    3.79,
    3.77,
    3.75,

    3.73,
    3.71,
    3.69,
    3.61,
    3.50,

    3.30
  };


  const int percentages[] = {

    100,
    95,
    90,
    85,
    80,

    75,
    70,
    65,
    60,
    55,

    50,
    45,
    40,
    35,
    30,

    25,
    20,
    15,
    10,
    5,

    0
  };


  const int count =
    sizeof(voltages)
    /
    sizeof(voltages[0]);


  if (
    voltage >=
    voltages[0]
  ) {

    return 100;
  }


  if (
    voltage <=
    voltages[count - 1]
  ) {

    return 0;
  }


  for (
    int i = 0;
    i < count - 1;
    i++
  ) {

    float highVoltage =
      voltages[i];


    float lowVoltage =
      voltages[i + 1];


    if (
      voltage <= highVoltage
      &&
      voltage >= lowVoltage
    ) {

      int highPercent =
        percentages[i];


      int lowPercent =
        percentages[i + 1];


      float ratio =
        (
          voltage
          -
          lowVoltage
        )
        /
        (
          highVoltage
          -
          lowVoltage
        );


      int result =
        lowPercent
        +
        round(
          ratio
          *
          (
            highPercent
            -
            lowPercent
          )
        );


      return constrain(
        result,
        0,
        100
      );
    }
  }


  return 0;
}


// ======================================================
// POWER SOURCE
// ======================================================

const char *powerSourceCode(
  PowerSource source
) {

  switch (
    source
  ) {

    case POWER_BATTERY:

      return "BAT";


    case POWER_EXTERNAL:

      return "USB";


    default:

      return "UNK";
  }
}


// ======================================================
// SAFETY STATE
// ======================================================

BatterySafetyState getBatterySafetyState(
  float voltage
) {

  if (
    voltage <=
    CRITICAL_BATTERY_VOLTAGE
  ) {

    return BATTERY_CRITICAL;
  }


  if (
    voltage <=
    LOW_BATTERY_VOLTAGE
  ) {

    return BATTERY_LOW;
  }


  return BATTERY_SAFE;
}


// ------------------------------------------------------

const char *batterySafetyStateCode(
  BatterySafetyState state
) {

  switch (
    state
  ) {

    case BATTERY_LOW:

      return "LOW";


    case BATTERY_CRITICAL:

      return "CRITICAL";


    default:

      return "NORMAL";
  }
}


// ======================================================
// RUNTIME
// ======================================================

unsigned long getRuntimeSecondsUnsafe() {

  if (
    runtimeActive
  ) {

    return
      runtimeFrozenSeconds
      +
      (
        millis()
        -
        runtimeStartMs
      )
      /
      1000UL;
  }


  return
    runtimeFrozenSeconds;
}


// ------------------------------------------------------

void startNewRuntimeSessionUnsafe(
  int startPercent,
  int startMilliVolts
) {

  runtimeActive =
    true;


  runtimeStartMs =
    millis();


  runtimeFrozenSeconds =
    0;


  runtimeStartPercent =
    startPercent;


  runtimeStartMilliVolts =
    startMilliVolts;
}


// ------------------------------------------------------

void stopRuntimeSessionUnsafe() {

  if (
    !runtimeActive
  ) {

    return;
  }


  runtimeFrozenSeconds =
    getRuntimeSecondsUnsafe();


  runtimeActive =
    false;
}


// ------------------------------------------------------

void resetRuntimeSession() {

  if (
    xSemaphoreTake(
      stateMutex,
      pdMS_TO_TICKS(100)
    )
    ==
    pdTRUE
  ) {

    runtimeFrozenSeconds =
      0;


    if (
      powerSource ==
      POWER_BATTERY
    ) {

      runtimeActive =
        true;


      runtimeStartMs =
        millis();


      runtimeStartPercent =
        batteryPercent;


      runtimeStartMilliVolts =
        round(
          batteryVoltage
          *
          1000.0f
        );

    } else {

      runtimeActive =
        false;


      runtimeStartPercent =
        -1;


      runtimeStartMilliVolts =
        0;
    }


    xSemaphoreGive(
      stateMutex
    );
  }


  debugLine(
    "Runtime test reset"
  );
}


// ======================================================
// LOW BATTERY SCREEN
// ======================================================

void showLowBatteryShutdownScreen() {

  digitalWrite(
    TFT_BL,
    HIGH
  );


  sprite.fillSprite(
    TFT_BLACK
  );


  sprite.setTextDatum(
    MC_DATUM
  );


  sprite.setTextColor(
    TFT_RED,
    TFT_BLACK
  );


  sprite.drawString(
    "LOW BATTERY",
    sprite.width() / 2,
    sprite.height() / 2 - 15,
    2
  );


  sprite.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );


  sprite.drawString(
    "Please charge",
    sprite.width() / 2,
    sprite.height() / 2 + 15,
    2
  );


  pushScaledSprite();


  delay(
    2500
  );
}


// ======================================================
// LOW BATTERY PROTECTION
// ======================================================

void enterLowBatteryProtection() {

  if (
    DEBUG_MODE
  ) {

    Serial.println(
      "LOW BATTERY PROTECTION"
    );
  }


  showLowBatteryShutdownScreen();


  digitalWrite(
    TFT_BL,
    LOW
  );


  digitalWrite(
    BATTERY_ADC_EN,
    LOW
  );


  if (
    DEBUG_MODE
  ) {

    Serial.flush();
  }


  while (
    digitalRead(
      POWER_BUTTON
    )
    ==
    LOW
  ) {

    delay(
      10
    );
  }


  delay(
    200
  );


  esp_sleep_enable_ext0_wakeup(
    GPIO_NUM_35,
    0
  );


  esp_deep_sleep_start();
}


// ======================================================
// PROCESS BATTERY PROTECTION
// ======================================================

void processBatteryProtection(
  PowerSource source,
  float measuredVoltage
) {

  if (
    source !=
    POWER_BATTERY
  ) {

    lowBatteryConfirmCount =
      0;


    return;
  }


  float shutdownVoltage =
    LOW_BATTERY_TEST_MODE
      ? TEST_SHUTDOWN_VOLTAGE
      : BATTERY_SHUTDOWN_VOLTAGE;


  if (
    measuredVoltage <=
    shutdownVoltage
  ) {

    lowBatteryConfirmCount++;

  } else {

    lowBatteryConfirmCount =
      0;
  }


  if (
    DEBUG_MODE
  ) {

    Serial.print(
      "Battery: "
    );


    Serial.print(
      measuredVoltage,
      3
    );


    Serial.print(
      " V | Safety: "
    );


    Serial.print(
      batterySafetyStateCode(
        getBatterySafetyState(
          measuredVoltage
        )
      )
    );


    Serial.print(
      " | Protection: "
    );


    Serial.print(
      lowBatteryConfirmCount
    );


    Serial.print(
      "/"
    );


    Serial.println(
      LOW_BATTERY_CONFIRM_COUNT
    );
  }


  if (
    lowBatteryConfirmCount >=
    LOW_BATTERY_CONFIRM_COUNT
  ) {

    enterLowBatteryProtection();
  }
}


// ======================================================
// UPDATE POWER
// ======================================================

void updatePowerReading() {

  float measuredVoltage =
    readPowerSenseVoltage();


  PowerSource newSource =
    POWER_UNKNOWN;


  float newBatteryVoltage =
    0.0f;


  int newBatteryPercent =
    -1;


  BatterySafetyState newSafetyState =
    BATTERY_SAFE;


  // ==================================================
  // BATTERY
  // ==================================================

  if (
    measuredVoltage >= 2.50f
    &&
    measuredVoltage <=
    EXTERNAL_POWER_THRESHOLD
  ) {

    newSource =
      POWER_BATTERY;


    newBatteryVoltage =
      measuredVoltage;


    newBatteryPercent =
      voltageToPercent(
        measuredVoltage
      );


    newSafetyState =
      getBatterySafetyState(
        measuredVoltage
      );
  }


  // ==================================================
  // USB
  // ==================================================

  else if (
    measuredVoltage >
    EXTERNAL_POWER_THRESHOLD
    &&
    measuredVoltage <= 5.50f
  ) {

    newSource =
      POWER_EXTERNAL;
  }


  // ==================================================
  // UPDATE SHARED STATE
  // ==================================================

  if (
    xSemaphoreTake(
      stateMutex,
      pdMS_TO_TICKS(100)
    )
    ==
    pdTRUE
  ) {

    PowerSource oldSource =
      powerSource;


    if (
      newSource ==
      POWER_BATTERY
      &&
      oldSource !=
      POWER_BATTERY
    ) {

      startNewRuntimeSessionUnsafe(
        newBatteryPercent,

        round(
          newBatteryVoltage
          *
          1000.0f
        )
      );
    }


    if (
      newSource !=
      POWER_BATTERY
      &&
      oldSource ==
      POWER_BATTERY
    ) {

      stopRuntimeSessionUnsafe();
    }


    powerSenseVoltage =
      measuredVoltage;


    powerSource =
      newSource;


    batteryVoltage =
      newBatteryVoltage;


    batteryPercent =
      newBatteryPercent;


    batterySafetyState =
      newSafetyState;


    xSemaphoreGive(
      stateMutex
    );

  } else {

    powerSenseVoltage =
      measuredVoltage;


    powerSource =
      newSource;


    batteryVoltage =
      newBatteryVoltage;


    batteryPercent =
      newBatteryPercent;


    batterySafetyState =
      newSafetyState;
  }


  processBatteryProtection(
    newSource,
    newBatteryVoltage
  );
}


// ======================================================
// BLE CALLBACK
// ======================================================

class CommandCallbacks :
  public BLECharacteristicCallbacks {

  void onWrite(
    BLECharacteristic *pCharacteristic
  ) override {

    String value =
      pCharacteristic->getValue();


    value.trim();


    if (
      value.length() == 0
    ) {

      return;
    }


    if (
      DEBUG_MODE
    ) {

      Serial.print(
        "Received: "
      );


      Serial.println(
        value
      );
    }


    if (
      xSemaphoreTake(
        commandMutex,
        pdMS_TO_TICKS(100)
      )
      ==
      pdTRUE
    ) {

      pendingCommand =
        value;


      newCommandReady =
        true;


      xSemaphoreGive(
        commandMutex
      );
    }
  }


  // ----------------------------------------------------
  // READ STATE
  // ----------------------------------------------------

  void onRead(
    BLECharacteristic *pCharacteristic
  ) override {

    String currentMessage;


    bool currentScroll;

    bool currentDisplay;


    PowerSource currentPowerSource;


    int currentBatteryPercent;

    int currentBatteryMilliVolts;

    int currentSenseMilliVolts;


    BatterySafetyState currentSafety;


    bool currentRuntimeActive;

    unsigned long currentRuntimeSeconds;


    int currentRuntimeStartPercent;

    int currentRuntimeStartMilliVolts;


    if (
      xSemaphoreTake(
        stateMutex,
        pdMS_TO_TICKS(100)
      )
      ==
      pdTRUE
    ) {

      currentMessage =
        message;


      currentScroll =
        scrollEnabled;


      currentDisplay =
        displayEnabled;


      currentPowerSource =
        powerSource;


      currentBatteryPercent =
        batteryPercent;


      currentBatteryMilliVolts =
        round(
          batteryVoltage
          *
          1000.0f
        );


      currentSenseMilliVolts =
        round(
          powerSenseVoltage
          *
          1000.0f
        );


      currentSafety =
        batterySafetyState;


      currentRuntimeActive =
        runtimeActive;


      currentRuntimeSeconds =
        getRuntimeSecondsUnsafe();


      currentRuntimeStartPercent =
        runtimeStartPercent;


      currentRuntimeStartMilliVolts =
        runtimeStartMilliVolts;


      xSemaphoreGive(
        stateMutex
      );

    } else {

      currentMessage =
        message;


      currentScroll =
        scrollEnabled;


      currentDisplay =
        displayEnabled;


      currentPowerSource =
        powerSource;


      currentBatteryPercent =
        batteryPercent;


      currentBatteryMilliVolts =
        round(
          batteryVoltage
          *
          1000.0f
        );


      currentSenseMilliVolts =
        round(
          powerSenseVoltage
          *
          1000.0f
        );


      currentSafety =
        batterySafetyState;


      currentRuntimeActive =
        runtimeActive;


      currentRuntimeSeconds =
        getRuntimeSecondsUnsafe();


      currentRuntimeStartPercent =
        runtimeStartPercent;


      currentRuntimeStartMilliVolts =
        runtimeStartMilliVolts;
    }


    String safetyCode =
      currentPowerSource ==
      POWER_BATTERY
        ? batterySafetyStateCode(
            currentSafety
          )
        : "NA";


    // ==================================================
    // V2.12 STATE
    //
    // 0 scroll
    // 1 display
    // 2 source
    // 3 battery %
    // 4 battery mV
    // 5 sense mV
    // 6 runtime active
    // 7 runtime seconds
    // 8 runtime start %
    // 9 runtime start mV
    // 10 safety
    // 11 firmware
    //
    // ==================================================

    String state =
      String(
        currentScroll
          ? "1"
          : "0"
      )
      +
      ","
      +
      String(
        currentDisplay
          ? "1"
          : "0"
      )
      +
      ","
      +
      powerSourceCode(
        currentPowerSource
      )
      +
      ","
      +
      String(
        currentBatteryPercent
      )
      +
      ","
      +
      String(
        currentBatteryMilliVolts
      )
      +
      ","
      +
      String(
        currentSenseMilliVolts
      )
      +
      ","
      +
      String(
        currentRuntimeActive
          ? "1"
          : "0"
      )
      +
      ","
      +
      String(
        currentRuntimeSeconds
      )
      +
      ","
      +
      String(
        currentRuntimeStartPercent
      )
      +
      ","
      +
      String(
        currentRuntimeStartMilliVolts
      )
      +
      ","
      +
      safetyCode
      +
      ","
      +
      FIRMWARE_VERSION
      +
      "\n"
      +
      currentMessage;


    pCharacteristic->setValue(
      state.c_str()
    );
  }
};


// ======================================================
// BLE SERVER CALLBACK
// ======================================================

class ServerCallbacks :
  public BLEServerCallbacks {

  void onConnect(
    BLEServer *pServer
  ) override {

    debugLine(
      "BLE connected"
    );
  }


  void onDisconnect(
    BLEServer *pServer
  ) override {

    debugLine(
      "BLE disconnected"
    );


    pServer
      ->getAdvertising()
      ->start();
  }
};


// ======================================================
// DEEP SLEEP
// ======================================================

void goToSleep() {

  debugLine(
    "Entering deep sleep"
  );


  digitalWrite(
    TFT_BL,
    LOW
  );


  digitalWrite(
    BATTERY_ADC_EN,
    LOW
  );


  if (
    DEBUG_MODE
  ) {

    Serial.flush();
  }


  while (
    digitalRead(
      POWER_BUTTON
    )
    ==
    LOW
  ) {

    delay(
      10
    );
  }


  delay(
    200
  );


  esp_sleep_enable_ext0_wakeup(
    GPIO_NUM_35,
    0
  );


  esp_deep_sleep_start();
}


// ======================================================
// SET MESSAGE
// ======================================================

void setMessage(
  const String &newMessage
) {

  if (
    newMessage.length() == 0
  ) {

    return;
  }


  int newWidth =
    measureRichText(
      newMessage
    );


  bool forcedScroll =
    false;


  if (
    xSemaphoreTake(
      stateMutex,
      pdMS_TO_TICKS(100)
    )
    ==
    pdTRUE
  ) {

    message =
      newMessage;


    textWidth =
      newWidth;


    if (
      !scrollEnabled
      &&
      textWidth >
      sprite.width()
    ) {

      scrollEnabled =
        true;


      forcedScroll =
        true;
    }


    xSemaphoreGive(
      stateMutex
    );

  } else {

    message =
      newMessage;


    textWidth =
      newWidth;


    if (
      !scrollEnabled
      &&
      textWidth >
      sprite.width()
    ) {

      scrollEnabled =
        true;


      forcedScroll =
        true;
    }
  }


  preferences.putString(
    "message",
    message
  );


  if (
    forcedScroll
  ) {

    preferences.putBool(
      "scroll",
      true
    );
  }


  if (
    scrollEnabled
  ) {

    x =
      sprite.width();

  } else {

    x =
      (
        sprite.width()
        -
        textWidth
      )
      /
      2;
  }
}


// ======================================================
// DISPLAY
// ======================================================

void setDisplay(
  bool enabled
) {

  if (
    xSemaphoreTake(
      stateMutex,
      pdMS_TO_TICKS(100)
    )
    ==
    pdTRUE
  ) {

    displayEnabled =
      enabled;


    xSemaphoreGive(
      stateMutex
    );

  } else {

    displayEnabled =
      enabled;
  }


  preferences.putBool(
    "display",
    enabled
  );


  digitalWrite(
    TFT_BL,
    enabled
      ? HIGH
      : LOW
  );
}


// ======================================================
// SCROLL
// ======================================================

void setScroll(
  bool enabled
) {

  if (
    enabled
  ) {

    if (
      xSemaphoreTake(
        stateMutex,
        pdMS_TO_TICKS(100)
      )
      ==
      pdTRUE
    ) {

      scrollEnabled =
        true;


      xSemaphoreGive(
        stateMutex
      );

    } else {

      scrollEnabled =
        true;
    }


    preferences.putBool(
      "scroll",
      true
    );


    x =
      sprite.width();


    return;
  }


  // ข้อความยาว
  // ห้ามปิด Scroll
  if (
    textWidth >
    sprite.width()
  ) {

    if (
      xSemaphoreTake(
        stateMutex,
        pdMS_TO_TICKS(100)
      )
      ==
      pdTRUE
    ) {

      scrollEnabled =
        true;


      xSemaphoreGive(
        stateMutex
      );

    } else {

      scrollEnabled =
        true;
    }


    preferences.putBool(
      "scroll",
      true
    );


    return;
  }


  if (
    xSemaphoreTake(
      stateMutex,
      pdMS_TO_TICKS(100)
    )
    ==
    pdTRUE
  ) {

    scrollEnabled =
      false;


    xSemaphoreGive(
      stateMutex
    );

  } else {

    scrollEnabled =
      false;
  }


  preferences.putBool(
    "scroll",
    false
  );


  x =
    (
      sprite.width()
      -
      textWidth
    )
    /
    2;
}


// ======================================================
// COMMAND HANDLER
// ======================================================

void handleCommand(
  String command
) {

  command.trim();


  if (
    command.length() == 0
  ) {

    return;
  }


  // ==================================================
  // TEXT
  // ==================================================

  if (
    command.length() >= 5
  ) {

    String prefix =
      command.substring(
        0,
        5
      );


    prefix.toUpperCase();


    if (
      prefix ==
      "TEXT:"
    ) {

      String newMessage =
        command.substring(
          5
        );


      newMessage.trim();


      if (
        newMessage.length() > 0
      ) {

        setMessage(
          newMessage
        );
      }


      return;
    }
  }


  // ==================================================
  // DISPLAY
  // ==================================================

  if (
    command.equalsIgnoreCase(
      "DISPLAY:ON"
    )
  ) {

    setDisplay(
      true
    );

    return;
  }


  if (
    command.equalsIgnoreCase(
      "DISPLAY:OFF"
    )
  ) {

    setDisplay(
      false
    );

    return;
  }


  // ==================================================
  // SCROLL
  // ==================================================

  if (
    command.equalsIgnoreCase(
      "SCROLL:ON"
    )
  ) {

    setScroll(
      true
    );

    return;
  }


  if (
    command.equalsIgnoreCase(
      "SCROLL:OFF"
    )
  ) {

    setScroll(
      false
    );

    return;
  }


  // ==================================================
  // DIAGNOSTICS
  // ==================================================

  if (
    command.equalsIgnoreCase(
      "RUNTIME:RESET"
    )
  ) {

    resetRuntimeSession();

    return;
  }


  // ==================================================
  // FALLBACK
  // ==================================================

  setMessage(
    command
  );
}


// ======================================================
// SCALE SPRITE TO DISPLAY
// ======================================================

void pushScaledSprite() {

  if (
    TEXT_SCALE <= 1
  ) {

    sprite.pushSprite(
      0,
      0
    );

    return;
  }


  const int sourceWidth =
    sprite.width();


  const int sourceHeight =
    sprite.height();


  const int outputWidth =
    sourceWidth
    *
    TEXT_SCALE;


  const int outputHeight =
    sourceHeight
    *
    TEXT_SCALE;


  const int offsetX =
    (
      tft.width()
      -
      outputWidth
    )
    /
    2;


  const int offsetY =
    (
      tft.height()
      -
      outputHeight
    )
    /
    2;


  static uint16_t scaledRow[240];


  for (
    int sourceY = 0;
    sourceY < sourceHeight;
    sourceY++
  ) {

    int outputX =
      0;


    for (
      int sourceX = 0;
      sourceX < sourceWidth;
      sourceX++
    ) {

      uint16_t color =
        sprite.readPixel(
          sourceX,
          sourceY
        );


      for (
        int scaleX = 0;
        scaleX < TEXT_SCALE;
        scaleX++
      ) {

        scaledRow[outputX++] =
          color;
      }
    }


    for (
      int scaleY = 0;
      scaleY < TEXT_SCALE;
      scaleY++
    ) {

      tft.pushImage(

        offsetX,

        offsetY
          +
        sourceY
          *
        TEXT_SCALE
          +
        scaleY,

        outputWidth,

        1,

        scaledRow
      );
    }
  }
}


// ======================================================
// SETUP
// ======================================================

void setup() {

  if (
    DEBUG_MODE
  ) {

    Serial.begin(
      115200
    );


    delay(
      200
    );


    Serial.println();


    Serial.print(
      "HairClip firmware "
    );


    Serial.println(
      FIRMWARE_VERSION
    );
  }


  // ==================================================
  // BUTTON
  // ==================================================

  pinMode(
    POWER_BUTTON,
    INPUT
  );


  // ==================================================
  // NVS
  // ==================================================

  preferences.begin(
    "hairclip",
    false
  );


  message =
    preferences.getString(
      "message",
      "HELLO WORLD!"
    );


  displayEnabled =
    preferences.getBool(
      "display",
      true
    );


  scrollEnabled =
    preferences.getBool(
      "scroll",
      true
    );


  // ==================================================
  // MUTEX
  // ==================================================

  commandMutex =
    xSemaphoreCreateMutex();


  stateMutex =
    xSemaphoreCreateMutex();


  // ==================================================
  // BATTERY ADC
  // ==================================================

  pinMode(
    BATTERY_ADC_EN,
    OUTPUT
  );


  digitalWrite(
    BATTERY_ADC_EN,
    HIGH
  );


  pinMode(
    BATTERY_ADC_PIN,
    INPUT
  );


  analogReadResolution(
    12
  );


  analogSetPinAttenuation(
    BATTERY_ADC_PIN,
    ADC_11db
  );


  delay(
    500
  );


  // ==================================================
  // DISPLAY
  // ==================================================

  pinMode(
    TFT_BL,
    OUTPUT
  );


  digitalWrite(
    TFT_BL,
    displayEnabled
      ? HIGH
      : LOW
  );


  tft.init();


  tft.setRotation(
    1
  );


  tft.fillScreen(
    TFT_BLACK
  );


  sprite.setColorDepth(
    16
  );


  sprite.createSprite(
  tft.width() / TEXT_SCALE,
  tft.height() / TEXT_SCALE
);


  sprite.fillSprite(
    TFT_BLACK
  );


  // ==================================================
  // FONT
  // ==================================================

  u8f.begin(
    sprite
  );


  u8f.setFontMode(
    1
  );


  u8f.setFontDirection(
    0
  );


  u8f.setForegroundColor(
    TFT_WHITE
  );


  u8f.setFont(
    u8g2_font_etl24thai_t
  );


  // ==================================================
  // MESSAGE
  // ==================================================

  textWidth =
    measureRichText(
      message
    );


  if (
    !scrollEnabled
    &&
    textWidth >
    sprite.width()
  ) {

    scrollEnabled =
      true;


    preferences.putBool(
      "scroll",
      true
    );
  }


  if (
    scrollEnabled
  ) {

    x =
      sprite.width();

  } else {

    x =
      (
        sprite.width()
        -
        textWidth
      )
      /
      2;
  }


  // ==================================================
  // POWER
  // ==================================================

  updatePowerReading();


  lastPowerRead =
    millis();


  // ==================================================
  // BLE
  // ==================================================

  BLEDevice::init(
    "HairClip-V1"
  );


  BLEServer *pServer =
    BLEDevice::createServer();


  pServer->setCallbacks(
    new ServerCallbacks()
  );


  BLEService *pService =
    pServer->createService(
      SERVICE_UUID
    );


  BLECharacteristic *pCharacteristic =
    pService->createCharacteristic(

      CHARACTERISTIC_UUID,

      BLECharacteristic::PROPERTY_READ
      |
      BLECharacteristic::PROPERTY_WRITE
      |
      BLECharacteristic::PROPERTY_WRITE_NR
    );


  pCharacteristic->setCallbacks(
    new CommandCallbacks()
  );


  pService->start();


  BLEAdvertising *pAdvertising =
    pServer->getAdvertising();


  pAdvertising->addServiceUUID(
    SERVICE_UUID
  );


  pAdvertising->start();


  debugLine(
    "HairClip ready"
  );
}


// ======================================================
// LOOP
// ======================================================

void loop() {

  // ==================================================
  // PHYSICAL POWER BUTTON
  // ==================================================

  if (
    digitalRead(
      POWER_BUTTON
    )
    ==
    LOW
  ) {

    delay(
      50
    );


    if (
      digitalRead(
        POWER_BUTTON
      )
      ==
      LOW
    ) {

      goToSleep();
    }
  }


  // ==================================================
  // BLE COMMAND
  // ==================================================

  String command =
    "";


  bool executeCommand =
    false;


  if (
    xSemaphoreTake(
      commandMutex,
      0
    )
    ==
    pdTRUE
  ) {

    if (
      newCommandReady
    ) {

      command =
        pendingCommand;


      newCommandReady =
        false;


      executeCommand =
        true;
    }


    xSemaphoreGive(
      commandMutex
    );
  }


  if (
    executeCommand
  ) {

    handleCommand(
      command
    );
  }


  // ==================================================
  // POWER UPDATE
  // ==================================================

  if (
    millis()
    -
    lastPowerRead
    >=
    POWER_READ_INTERVAL
  ) {

    updatePowerReading();


    lastPowerRead =
      millis();
  }


  // ==================================================
  // DISPLAY
  // ==================================================

  if (
    displayEnabled
  ) {

    sprite.fillSprite(
      TFT_BLACK
    );


    int baselineY =
      getTextBaselineY();


    drawRichText(
      x,
      baselineY,
      message
    );


    pushScaledSprite();


    if (
      scrollEnabled
    ) {

      x -=
        scrollSpeed;


      if (
        x <
        -textWidth
      ) {

        x =
          sprite.width();
      }
    }
  }


  delay(
    frameDelay
  );
}