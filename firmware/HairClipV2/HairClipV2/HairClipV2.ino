#include <TFT_eSPI.h>
#include <U8g2_for_TFT_eSPI.h>

#include <esp_sleep.h>
#include <Preferences.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


// ======================================================
// HARDWARE
// ======================================================

#define TFT_BL 4
#define POWER_BUTTON 35


// ======================================================
// DISPLAY
// ======================================================

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// U8g2 จะวาดลง Sprite แทนวาดลงจอโดยตรง
U8g2_for_TFT_eSPI u8f;

String message = "HELLO WORLD!";

int x = 0;
int textWidth = 0;

const int scrollSpeed = 2;
const int frameDelay = 25;

bool displayEnabled = true;
bool scrollEnabled = true;


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
// THAI SHAPING
// ======================================================

// เครื่องหมายภาษาไทยที่ต้องวาดซ้อนกับตัวอักษรก่อนหน้า
bool isThaiCombiningMark(uint16_t codepoint) {

  // ั
  if (codepoint == 0x0E31) {
    return true;
  }

  // ิ ี ึ ื ุ ู ฺ
  if (
    codepoint >= 0x0E34 &&
    codepoint <= 0x0E3A
  ) {
    return true;
  }

  // ็ ่ ้ ๊ ๋ ์ ํ ๎
  if (
    codepoint >= 0x0E47 &&
    codepoint <= 0x0E4E
  ) {
    return true;
  }

  return false;
}


// ======================================================
// UTF-8 DECODER
// ======================================================

uint16_t nextUTF8(const char *&p) {

  uint8_t c =
    (uint8_t)*p++;

  // ASCII
  if (c < 0x80) {
    return c;
  }

  // 2-byte UTF-8
  if (
    (c & 0xE0) == 0xC0
  ) {

    uint16_t result =
      (c & 0x1F) << 6;

    result |=
      (
        (uint8_t)*p++
        & 0x3F
      );

    return result;
  }

  // 3-byte UTF-8
  // ภาษาไทยอยู่ในกลุ่มนี้
  if (
    (c & 0xF0) == 0xE0
  ) {

    uint16_t result =
      (c & 0x0F) << 12;

    result |=
      (
        (
          (uint8_t)*p++
          & 0x3F
        )
        << 6
      );

    result |=
      (
        (uint8_t)*p++
        & 0x3F
      );

    return result;
  }

  // Emoji ส่วนใหญ่เป็น Unicode เกิน 16-bit
  // รอบนี้ยังไม่รองรับ
  if (
    (c & 0xF8) == 0xF0
  ) {

    // ข้ามอีก 3 byte
    p += 3;

    return '?';
  }

  return '?';
}


// ======================================================
// CODEPOINT -> UTF-8
// ใช้สำหรับวัดความกว้าง glyph
// ======================================================

void codepointToUTF8(
  uint16_t codepoint,
  char *buffer
) {

  buffer[0] = '\0';
  buffer[1] = '\0';
  buffer[2] = '\0';
  buffer[3] = '\0';

  if (codepoint < 0x80) {

    buffer[0] =
      (char)codepoint;

    return;
  }

  if (codepoint < 0x800) {

    buffer[0] =
      0xC0 |
      (codepoint >> 6);

    buffer[1] =
      0x80 |
      (codepoint & 0x3F);

    return;
  }

  buffer[0] =
    0xE0 |
    (codepoint >> 12);

  buffer[1] =
    0x80 |
    (
      (codepoint >> 6)
      & 0x3F
    );

  buffer[2] =
    0x80 |
    (codepoint & 0x3F);
}


// ======================================================
// GLYPH WIDTH
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


// ======================================================
// MEASURE THAI / UTF-8 TEXT
// ======================================================

int measureShapedText(
  const String &text
) {

  const char *p =
    text.c_str();

  int width = 0;

  while (*p) {

    uint16_t codepoint =
      nextUTF8(p);

    // สระบน/ล่าง/วรรณยุกต์
    // ไม่เพิ่มความกว้าง
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
// DRAW THAI / UTF-8 TEXT
// ======================================================

int drawShapedText(
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

    uint16_t codepoint =
      nextUTF8(p);


    // --------------------------------------------------
    // Combining Mark
    // --------------------------------------------------

    if (
      isThaiCombiningMark(
        codepoint
      )
    ) {

      // วาดซ้อนกับตัวก่อนหน้า
      // โดยไม่ขยับ Cursor
      u8f.drawGlyph(
        baseX,
        baselineY,
        codepoint
      );

      continue;
    }


    // --------------------------------------------------
    // Normal Character
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


  return cursorX - startX;
}


// ======================================================
// CALCULATE VERTICAL CENTER
// ======================================================

int getTextBaselineY() {

  int ascent =
    u8f.getFontAscent();

  int descent =
    u8f.getFontDescent();

  int fontHeight =
    ascent - descent;

  return
    (
      (
        sprite.height()
        - fontHeight
      )
      / 2
    )
    + ascent;
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
      ) == pdTRUE
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
  // Controller อ่าน State
  // ----------------------------------------------------

  void onRead(
    BLECharacteristic *pCharacteristic
  ) override {

    String currentMessage;
    bool currentScroll;
    bool currentDisplay;


    if (
      xSemaphoreTake(
        stateMutex,
        pdMS_TO_TICKS(100)
      ) == pdTRUE
    ) {

      currentMessage =
        message;

      currentScroll =
        scrollEnabled;

      currentDisplay =
        displayEnabled;

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
    }


    String state =
      String(
        currentScroll
          ? "1"
          : "0"
      )
      + ","
      + String(
        currentDisplay
          ? "1"
          : "0"
      )
      + "\n"
      + currentMessage;


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


  digitalWrite(
    TFT_BL,
    LOW
  );


  while (
    digitalRead(
      POWER_BUTTON
    ) == LOW
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


  if (
    xSemaphoreTake(
      stateMutex,
      pdMS_TO_TICKS(100)
    ) == pdTRUE
  ) {

    message =
      newMessage;

    xSemaphoreGive(
      stateMutex
    );

  } else {

    message =
      newMessage;
  }


  // บันทึก UTF-8 ลง NVS
  preferences.putString(
    "message",
    newMessage
  );


  textWidth =
    measureShapedText(
      newMessage
    );


  x =
    sprite.width();


  Serial.print(
    "Message changed: "
  );

  Serial.println(
    newMessage
  );


  Serial.print(
    "Text width: "
  );

  Serial.println(
    textWidth
  );
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
    ) == pdTRUE
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

  if (
    xSemaphoreTake(
      stateMutex,
      pdMS_TO_TICKS(100)
    ) == pdTRUE
  ) {

    scrollEnabled =
      enabled;

    xSemaphoreGive(
      stateMutex
    );

  } else {

    scrollEnabled =
      enabled;
  }


  preferences.putBool(
    "scroll",
    enabled
  );


  if (enabled) {

    x =
      sprite.width();

  } else {

    x =
      (
        sprite.width()
        - textWidth
      )
      / 2;
  }


  Serial.print(
    "Scroll: "
  );

  Serial.println(
    enabled
      ? "ON"
      : "OFF"
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


  // ----------------------------------------------------
  // TEXT:
  // เปลี่ยนเฉพาะ prefix เป็นตัวใหญ่
  // ไม่แตะ UTF-8 ภาษาไทยที่อยู่ข้างหลัง
  // ----------------------------------------------------

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
      prefix == "TEXT:"
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


  // ----------------------------------------------------
  // DISPLAY
  // ----------------------------------------------------

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


  // ----------------------------------------------------
  // SCROLL
  // ----------------------------------------------------

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


  // ----------------------------------------------------
  // FALLBACK
  // ----------------------------------------------------

  // ส่งข้อความตรง ๆ ก็ยังใช้ได้
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

  delay(200);


  // ====================================================
  // BUTTON
  // ====================================================

  pinMode(
    POWER_BUTTON,
    INPUT
  );


  // ====================================================
  // STORAGE
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

  tft.setRotation(1);

  tft.fillScreen(
    TFT_BLACK
  );


  // 16-bit Sprite
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
  // U8G2 -> SPRITE
  // ====================================================

  u8f.begin(
    sprite
  );


  // Transparent Font
  // เพราะสระ/วรรณยุกต์ต้องวาดซ้อนกัน
  u8f.setFontMode(
    1
  );


  u8f.setFontDirection(
    0
  );


  u8f.setForegroundColor(
    TFT_WHITE
  );


  // Font ไทย + ASCII
  u8f.setFont(
    u8g2_font_etl24thai_t
  );


  // ====================================================
  // TEXT WIDTH
  // ====================================================

  textWidth =
    measureShapedText(
      message
    );


  if (
    scrollEnabled
  ) {

    x =
      sprite.width();

  } else {

    x =
      (
        sprite.width()
        - textWidth
      )
      / 2;
  }


  // ====================================================
  // DEBUG
  // ====================================================

  Serial.println();

  Serial.println(
    "Loaded state:"
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


  Serial.println();

  Serial.println(
    "HairClip V2.5 ready!"
  );

  Serial.println(
    "Thai UTF-8 enabled"
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
    ) == LOW
  ) {

    delay(50);


    if (
      digitalRead(
        POWER_BUTTON
      ) == LOW
    ) {

      goToSleep();
    }
  }


  // ====================================================
  // BLE COMMAND
  // ====================================================

  String command = "";

  bool executeCommand =
    false;


  if (
    xSemaphoreTake(
      commandMutex,
      0
    ) == pdTRUE
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
  // DISPLAY
  // ====================================================

  if (
    displayEnabled
  ) {

    // วาดเฟรมใหม่ใน RAM
    sprite.fillSprite(
      TFT_BLACK
    );


    int baselineY =
      getTextBaselineY();


    drawShapedText(
      x,
      baselineY,
      message
    );


    // ส่งทั้งเฟรมขึ้น TFT ทีเดียว
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
        x < -textWidth
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