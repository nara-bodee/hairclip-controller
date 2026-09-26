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

#define FIRMWARE_VERSION "2.10"


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
TFT_eSprite sprite = TFT_eSprite(&tft);

U8g2_for_TFT_eSPI u8f;

String message = "HELLO WORLD!";

int x = 0;
int textWidth = 0;

const int scrollSpeed = 2;
const int frameDelay = 25;

bool displayEnabled = true;
bool scrollEnabled = true;


// ======================================================
// ICON CONFIG
// ======================================================

const int ICON_WIDTH = 30;
const int ICON_GAP = 4;


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


// อ่านแรงดันทุก 5 วินาที
const unsigned long POWER_READ_INTERVAL =
  5000;


// จากการทดสอบจริง
// Battery ≈ 3.x–4.2V
// USB ≈ 4.6–4.7V
const float EXTERNAL_POWER_THRESHOLD =
  4.35f;


// ======================================================
// LOW BATTERY PROTECTION
// ======================================================

// เริ่มถือว่าแบตต่ำ
const float LOW_BATTERY_VOLTAGE =
  3.60f;


// เริ่ม Critical
const float CRITICAL_BATTERY_VOLTAGE =
  3.50f;


// ถ้าต่ำกว่านี้ต่อเนื่อง
// จะเข้า Deep Sleep
const float BATTERY_SHUTDOWN_VOLTAGE =
  3.45f;


// ต้องเจอต่ำกว่าจุด Shutdown
// 3 รอบติดกัน
const int LOW_BATTERY_CONFIRM_COUNT =
  3;


int lowBatteryConfirmCount =
  0;


// ======================================================
// LOW BATTERY TEST MODE
// ======================================================
//
// false = ใช้งานจริง
//
// true = ทดสอบระบบ Protection
//        โดยใช้ 4.15V เป็น shutdown threshold
//
// หลังทดสอบเสร็จต้องกลับเป็น false
//
// ======================================================

const bool LOW_BATTERY_TEST_MODE =
  false;


const float TEST_SHUTDOWN_VOLTAGE =
  4.15f;


// ======================================================
// BATTERY SAFETY STATE
// ======================================================

enum BatterySafetyState {
  BATTERY_SAFE,
  BATTERY_LOW,
  BATTERY_CRITICAL
};

BatterySafetyState batterySafetyState =
  BATTERY_SAFE;


// ======================================================
// RUNTIME TEST
// ======================================================

bool runtimeActive =
  false;


// เวลาที่ session ปัจจุบันเริ่ม
unsigned long runtimeStartMs =
  0;


// เวลาที่สะสมก่อน Pause
unsigned long runtimeFrozenSeconds =
  0;


// ค่าตอนเริ่ม Runtime Test
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


String pendingCommand = "";

bool newCommandReady =
  false;


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
  ) {

    return 7;
  }


  if (
    matchIconToken(
      p,
      ":star:"
    )
  ) {

    return 6;
  }


  if (
    matchIconToken(
      p,
      ":smile:"
    )
  ) {

    return 7;
  }


  if (
    matchIconToken(
      p,
      ":sun:"
    )
  ) {

    return 5;
  }


  if (
    matchIconToken(
      p,
      ":moon:"
    )
  ) {

    return 6;
  }


  if (
    matchIconToken(
      p,
      ":music:"
    )
  ) {

    return 7;
  }


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
// THAI COMBINING MARKS
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
// UTF-8 DECODER
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


  // Unicode Emoji 4-byte
  // ยังไม่ render ตรง ๆ
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


// ======================================================
// CODEPOINT -> UTF-8
// ======================================================

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
// TEXT BASELINE
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
      / 2
    )
    +
    ascent;
}


// ======================================================
// BATTERY ADC
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


  float voltage =
    (
      averageMilliVolts
      *
      2.0f
    )
    /
    1000.0f;


  return voltage;
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
// POWER SOURCE STRING
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
// BATTERY SAFETY
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


  Serial.println(
    "Runtime session started"
  );
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


  Serial.print(
    "Runtime session paused at "
  );


  Serial.print(
    runtimeFrozenSeconds
  );


  Serial.println(
    " seconds"
  );
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


  Serial.println(
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


  sprite.pushSprite(
    0,
    0
  );


  delay(
    2500
  );
}


// ======================================================
// LOW BATTERY PROTECTION
// ======================================================

void enterLowBatteryProtection() {

  Serial.println();

  Serial.println(
    "================================"
  );


  Serial.println(
    "LOW BATTERY PROTECTION TRIGGERED"
  );


  Serial.print(
    "Battery voltage: "
  );


  Serial.print(
    batteryVoltage,
    3
  );


  Serial.println(
    " V"
  );


  Serial.println(
    "Entering deep sleep..."
  );


  Serial.println(
    "================================"
  );


  showLowBatteryShutdownScreen();


  // ปิด Backlight
  digitalWrite(
    TFT_BL,
    LOW
  );


  // ปิดวงจรวัดแบต
  digitalWrite(
    BATTERY_ADC_EN,
    LOW
  );


  Serial.flush();


  // รอให้ปล่อยปุ่มก่อน
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


  // ปุ่มเดิมยังใช้ Wake ได้
  esp_sleep_enable_ext0_wakeup(
    GPIO_NUM_35,
    0
  );


  esp_deep_sleep_start();
}


// ======================================================
// PROCESS BATTERY SAFETY
// ======================================================

void processBatterySafety(
  float measuredVoltage
) {

  batterySafetyState =
    getBatterySafetyState(
      measuredVoltage
    );


  Serial.print(
    "Battery safety: "
  );


  Serial.println(
    batterySafetyStateCode(
      batterySafetyState
    )
  );


  float shutdownVoltage =
    LOW_BATTERY_TEST_MODE
      ? TEST_SHUTDOWN_VOLTAGE
      : BATTERY_SHUTDOWN_VOLTAGE;


  if (
    LOW_BATTERY_TEST_MODE
  ) {

    Serial.print(
      "TEST MODE shutdown threshold: "
    );


    Serial.print(
      shutdownVoltage,
      2
    );


    Serial.println(
      " V"
    );
  }


  // --------------------------------------------------
  // BELOW SHUTDOWN THRESHOLD
  // --------------------------------------------------

  if (
    measuredVoltage <=
    shutdownVoltage
  ) {

    lowBatteryConfirmCount++;


    Serial.print(
      "Low voltage confirmation: "
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

  else {

    if (
      lowBatteryConfirmCount > 0
    ) {

      Serial.println(
        "Low voltage confirmation cleared"
      );
    }


    lowBatteryConfirmCount =
      0;
  }


  // --------------------------------------------------
  // PROTECT
  // --------------------------------------------------

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
  }


  // ==================================================
  // USB / EXTERNAL
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
  // STATE UPDATE
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


    // ----------------------------------------------
    // BATTERY SESSION START
    // ----------------------------------------------

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


    // ----------------------------------------------
    // BATTERY SESSION STOP
    // ----------------------------------------------

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


    xSemaphoreGive(
      stateMutex
    );
  }


  // ==================================================
  // DEBUG
  // ==================================================

  Serial.print(
    "Power sense: "
  );


  Serial.print(
    measuredVoltage,
    3
  );


  Serial.print(
    " V | Source: "
  );


  Serial.println(
    powerSourceCode(
      newSource
    )
  );


  // ==================================================
  // BATTERY SAFETY
  // ==================================================

  if (
    newSource ==
    POWER_BATTERY
  ) {

    Serial.print(
      "Battery: "
    );


    Serial.print(
      newBatteryVoltage,
      3
    );


    Serial.print(
      " V / "
    );


    Serial.print(
      newBatteryPercent
    );


    Serial.println(
      "%"
    );


    processBatterySafety(
      newBatteryVoltage
    );
  }


  // ==================================================
  // USB
  // ==================================================

  else if (
    newSource ==
    POWER_EXTERNAL
  ) {

    lowBatteryConfirmCount =
      0;


    batterySafetyState =
      BATTERY_SAFE;


    Serial.println(
      "External power detected"
    );
  }


  // ==================================================
  // UNKNOWN
  // ==================================================

  else {

    lowBatteryConfirmCount =
      0;


    batterySafetyState =
      BATTERY_SAFE;


    Serial.println(
      "Power reading invalid"
    );
  }
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


    Serial.print(
      "Received: "
    );


    Serial.println(
      value
    );


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


      currentRuntimeActive =
        runtimeActive;


      currentRuntimeSeconds =
        getRuntimeSecondsUnsafe();


      currentRuntimeStartPercent =
        runtimeStartPercent;


      currentRuntimeStartMilliVolts =
        runtimeStartMilliVolts;
    }


    // ==================================================
    // STATE FORMAT
    //
    // scroll,
    // display,
    // source,
    // percent,
    // batteryMV,
    // senseMV,
    // runtimeActive,
    // runtimeSeconds,
    // runtimeStartPercent,
    // runtimeStartMV
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
      "\n"
      +
      currentMessage;


    pCharacteristic->setValue(
      state.c_str()
    );


    Serial.println(
      "State requested:"
    );


    Serial.println(
      state
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

    Serial.println(
      "BLE connected"
    );
  }


  void onDisconnect(
    BLEServer *pServer
  ) override {

    Serial.println(
      "BLE disconnected"
    );


    pServer
      ->getAdvertising()
      ->start();


    Serial.println(
      "BLE advertising restarted"
    );
  }
};


// ======================================================
// NORMAL DEEP SLEEP
// ======================================================

void goToSleep() {

  Serial.println(
    "Going to deep sleep..."
  );


  Serial.flush();


  digitalWrite(
    TFT_BL,
    LOW
  );


  digitalWrite(
    BATTERY_ADC_EN,
    LOW
  );


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


  Serial.print(
    "Message changed: "
  );


  Serial.println(
    message
  );


  Serial.print(
    "Rendered width: "
  );


  Serial.println(
    textWidth
  );


  if (
    forcedScroll
  ) {

    Serial.println(
      "Scrolling forced ON: message is wider than screen"
    );
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


  Serial.print(
    "Display: "
  );


  Serial.println(
    enabled
      ? "ON"
      : "OFF"
  );
}


// ======================================================
// SCROLL
// ======================================================

void setScroll(
  bool enabled
) {

  // --------------------------------------------------
  // ON
  // --------------------------------------------------

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


    Serial.println(
      "Scroll: ON"
    );


    return;
  }


  // --------------------------------------------------
  // OFF REJECTED
  // --------------------------------------------------

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


    Serial.println(
      "Scroll OFF rejected: message is wider than screen"
    );


    return;
  }


  // --------------------------------------------------
  // OFF
  // --------------------------------------------------

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


  Serial.println(
    "Scroll: OFF"
  );
}


// ======================================================
// COMMAND
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
  // RUNTIME
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
  // FALLBACK MESSAGE
  // ==================================================

  setMessage(
    command
  );
}


// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(
    115200
  );


  delay(
    200
  );


  Serial.println();

  Serial.println(
    "================================"
  );


  Serial.print(
    "HairClip V"
  );


  Serial.println(
    FIRMWARE_VERSION
  );


  Serial.println(
    "Low Battery Protection"
  );


  Serial.println(
    "================================"
  );


  if (
    LOW_BATTERY_TEST_MODE
  ) {

    Serial.println(
      "WARNING: LOW BATTERY TEST MODE IS ENABLED"
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
    tft.width(),
    tft.height()
  );


  sprite.fillSprite(
    TFT_BLACK
  );


  // ==================================================
  // U8G2
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
  // TEXT
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
  // INITIAL POWER READ
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


  // ==================================================
  // READY
  // ==================================================

  Serial.println();

  Serial.print(
    "HairClip V"
  );


  Serial.print(
    FIRMWARE_VERSION
  );


  Serial.println(
    " ready!"
  );


  Serial.print(
    "Message: "
  );


  Serial.println(
    message
  );


  Serial.print(
    "Scroll: "
  );


  Serial.println(
    scrollEnabled
      ? "ON"
      : "OFF"
  );


  Serial.print(
    "Display: "
  );


  Serial.println(
    displayEnabled
      ? "ON"
      : "OFF"
  );
}


// ======================================================
// LOOP
// ======================================================

void loop() {

  // ==================================================
  // POWER BUTTON
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


    sprite.pushSprite(
      0,
      0
    );


    // ================================================
    // SCROLL
    // ================================================

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