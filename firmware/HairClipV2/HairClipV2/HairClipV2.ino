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
// BATTERY
// ======================================================

float batteryVoltage = 0.0f;

int batteryPercent = -1;

bool batteryValid = false;

unsigned long lastBatteryRead = 0;

const unsigned long BATTERY_READ_INTERVAL =
  30000;


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

bool newCommandReady = false;


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

  const int points = 10;


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
      cos(angle)
      * 12;


    int y1 =
      cy
      +
      sin(angle)
      * 12;


    int x2 =
      cx
      +
      cos(angle)
      * 17;


    int y2 =
      cy
      +
      sin(angle)
      * 17;


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
// ICON TOKEN
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
// THAI SHAPING
// ======================================================

bool isThaiCombiningMark(
  uint16_t codepoint
) {

  // ั
  if (
    codepoint ==
    0x0E31
  ) {

    return true;
  }


  // ิ ี ึ ื ุ ู ฺ
  if (
    codepoint >= 0x0E34
    &&
    codepoint <= 0x0E3A
  ) {

    return true;
  }


  // ็ ่ ้ ๊ ๋ ์ ํ ๎
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


  // ASCII
  if (
    c < 0x80
  ) {

    return c;
  }


  // 2-byte UTF-8
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


  // 3-byte UTF-8
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


  // Emoji Unicode 4-byte
  // ยังไม่รองรับโดยตรง
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


  int width = 0;


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

    // --------------------------------------------------
    // ICON
    // --------------------------------------------------

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
        - 8;


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


    // --------------------------------------------------
    // NORMAL UTF-8
    // --------------------------------------------------

    uint16_t codepoint =
      nextUTF8(
        p
      );


    // --------------------------------------------------
    // THAI COMBINING MARK
    // --------------------------------------------------

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


    // --------------------------------------------------
    // NORMAL CHARACTER
    // --------------------------------------------------

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
// TEXT VERTICAL POSITION
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
// BATTERY VOLTAGE
// ======================================================
//
// IMPORTANT:
// GPIO14 is kept HIGH while device is running.
// This matches the BatteryDisplayTest that passed.
//
// ======================================================

float readBatteryVoltage() {

  const int samples = 20;


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


    delay(5);
  }


  float averageMilliVolts =
    totalMilliVolts
    /
    (float)samples;


  // T-Display battery divider ≈ 1:2
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
//
// เป็นค่าประมาณจาก LiPo voltage curve
// ไม่ใช่ fuel gauge จริง
//
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
// UPDATE BATTERY
// ======================================================

void updateBatteryReading() {

  float measuredVoltage =
    readBatteryVoltage();


  // Debug สำคัญ
  Serial.print(
    "Raw battery voltage: "
  );


  Serial.println(
    measuredVoltage,
    3
  );


  bool valid =
    measuredVoltage >= 2.5f
    &&
    measuredVoltage <= 4.5f;


  int measuredPercent =
    valid
      ? voltageToPercent(
          measuredVoltage
        )
      : -1;


  if (
    xSemaphoreTake(
      stateMutex,
      pdMS_TO_TICKS(100)
    )
    ==
    pdTRUE
  ) {

    batteryVoltage =
      measuredVoltage;


    batteryPercent =
      measuredPercent;


    batteryValid =
      valid;


    xSemaphoreGive(
      stateMutex
    );

  } else {

    batteryVoltage =
      measuredVoltage;


    batteryPercent =
      measuredPercent;


    batteryValid =
      valid;
  }


  if (
    batteryValid
  ) {

    Serial.print(
      "Battery: "
    );


    Serial.print(
      batteryVoltage,
      3
    );


    Serial.print(
      " V / "
    );


    Serial.print(
      batteryPercent
    );


    Serial.println(
      "%"
    );

  } else {

    Serial.println(
      "Battery: invalid reading"
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


    int currentBatteryPercent;

    int currentBatteryMilliVolts;


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


      currentBatteryPercent =
        batteryValid
          ? batteryPercent
          : -1;


      currentBatteryMilliVolts =
        batteryValid
          ? round(
              batteryVoltage
              *
              1000.0f
            )
          : 0;


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


      currentBatteryPercent =
        batteryValid
          ? batteryPercent
          : -1;


      currentBatteryMilliVolts =
        batteryValid
          ? round(
              batteryVoltage
              *
              1000.0f
            )
          : 0;
    }


    // ==================================================
    // BLE STATE FORMAT V2.8
    //
    // scroll,display,batteryPercent,batteryMilliVolts
    // message
    //
    // Example:
    //
    // 1,1,85,4080
    // สวัสดี :heart:
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
// DEEP SLEEP
// ======================================================

void goToSleep() {

  Serial.println(
    "Going to deep sleep..."
  );


  Serial.flush();


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


  while (
    digitalRead(
      POWER_BUTTON
    )
    ==
    LOW
  ) {

    delay(10);
  }


  delay(200);


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
      / 2;
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
// SET DISPLAY
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
// SET SCROLL
// ======================================================

void setScroll(
  bool enabled
) {

  // --------------------------------------------------
  // SCROLL ON
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
  // SCROLL OFF REQUEST
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
  // SCROLL OFF ACCEPTED
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
    / 2;


  Serial.println(
    "Scroll: OFF"
  );
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


  // --------------------------------------------------
  // TEXT:
  // --------------------------------------------------

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
        command.substring(5);


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


  // --------------------------------------------------
  // DISPLAY
  // --------------------------------------------------

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


  // --------------------------------------------------
  // SCROLL
  // --------------------------------------------------

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


  // --------------------------------------------------
  // FALLBACK MESSAGE
  // --------------------------------------------------

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


  // ====================================================
  // BUTTON
  // ====================================================

  pinMode(
    POWER_BUTTON,
    INPUT
  );


  // ====================================================
  // NVS
  // ====================================================

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


  // ====================================================
  // MUTEX
  // ====================================================

  commandMutex =
    xSemaphoreCreateMutex();


  stateMutex =
    xSemaphoreCreateMutex();


  // ====================================================
  // BATTERY ADC
  // ====================================================

  pinMode(
    BATTERY_ADC_EN,
    OUTPUT
  );


  // IMPORTANT:
  // เปิดวงจรวัดแบตค้างไว้
  // เหมือน BatteryDisplayTest ที่ผ่านแล้ว
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


  // ให้ ADC settle ก่อนอ่าน
  delay(
    500
  );


  // ====================================================
  // DISPLAY
  // ====================================================

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


  // ====================================================
  // U8G2
  // ====================================================

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


  // ====================================================
  // TEXT WIDTH
  // ====================================================

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
      / 2;
  }


  // ====================================================
  // INITIAL BATTERY READ
  // ====================================================

  updateBatteryReading();


  lastBatteryRead =
    millis();


  // ====================================================
  // BLE
  // ====================================================

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


  // ====================================================
  // DEBUG
  // ====================================================

  Serial.println();


  Serial.println(
    "HairClip V2.8.1 ready!"
  );


  Serial.println(
    "Thai + Icons + Battery ADC Fix"
  );


  Serial.print(
    "Message: "
  );


  Serial.println(
    message
  );


  Serial.print(
    "Text width: "
  );


  Serial.println(
    textWidth
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

  // ====================================================
  // POWER BUTTON
  // ====================================================

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


  // ====================================================
  // BLE COMMAND
  // ====================================================

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


  // ====================================================
  // BATTERY UPDATE
  // ====================================================

  if (
    millis()
    -
    lastBatteryRead
    >=
    BATTERY_READ_INTERVAL
  ) {

    updateBatteryReading();


    lastBatteryRead =
      millis();
  }


  // ====================================================
  // DISPLAY
  // ====================================================

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


    // ==================================================
    // SCROLL
    // ==================================================

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