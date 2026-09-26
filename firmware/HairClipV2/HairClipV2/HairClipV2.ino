#include <TFT_eSPI.h>
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

String message = "HELLO WORLD!";

int x = 0;
int textWidth = 0;

const int textSize = 3;
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
// BLE CHARACTERISTIC CALLBACK
// ======================================================

class CommandCallbacks : public BLECharacteristicCallbacks {

  // ----------------------------------------------------
  // WRITE
  // รับคำสั่งจาก Controller
  // ----------------------------------------------------

  void onWrite(BLECharacteristic *pCharacteristic) override {

    String value =
      pCharacteristic->getValue();

    value.trim();

    if (value.length() == 0) {
      return;
    }

    Serial.print("Received: ");
    Serial.println(value);


    if (
      xSemaphoreTake(
        commandMutex,
        pdMS_TO_TICKS(100)
      ) == pdTRUE
    ) {

      pendingCommand = value;
      newCommandReady = true;

      xSemaphoreGive(
        commandMutex
      );
    }
  }


  // ----------------------------------------------------
  // READ
  // ส่ง State ปัจจุบันกลับ Controller
  // ----------------------------------------------------

  void onRead(BLECharacteristic *pCharacteristic) override {

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
        currentScroll ? "1" : "0"
      )
      + ","
      + String(
        currentDisplay ? "1" : "0"
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

class ServerCallbacks : public BLEServerCallbacks {

  void onConnect(BLEServer *pServer) override {

    Serial.println(
      "BLE connected"
    );
  }


  void onDisconnect(BLEServer *pServer) override {

    Serial.println(
      "BLE disconnected"
    );

    // กลับมา Advertising ใหม่
    // เพื่อให้ Controller reconnect ได้
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


  // รอปล่อยปุ่มก่อน
  while (
    digitalRead(
      POWER_BUTTON
    ) == LOW
  ) {

    delay(10);
  }


  delay(200);


  // GPIO35 เป็น Wake source
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


  // บันทึกลง NVS
  preferences.putString(
    "message",
    newMessage
  );


  // คำนวณขนาดข้อความใหม่
  textWidth =
    sprite.textWidth(
      newMessage
    );


  // เริ่มจากด้านขวา
  x =
    sprite.width();


  Serial.print(
    "Message changed: "
  );

  Serial.println(
    newMessage
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

    // เริ่มวิ่งใหม่จากด้านขวา
    x =
      sprite.width();

  } else {

    // หยุดและจัดข้อความกลางจอ
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


  // ทำ copy สำหรับเปรียบเทียบคำสั่ง
  // โดยไม่ทำลายตัวพิมพ์ของ Message จริง
  String normalized =
    command;

  normalized.toUpperCase();


  // ====================================================
  // TEXT
  // ====================================================

  if (
    normalized.startsWith(
      "TEXT:"
    )
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


  // ====================================================
  // DISPLAY
  // ====================================================

  if (
    normalized ==
    "DISPLAY:ON"
  ) {

    setDisplay(
      true
    );

    return;
  }


  if (
    normalized ==
    "DISPLAY:OFF"
  ) {

    setDisplay(
      false
    );

    return;
  }


  // ====================================================
  // SCROLL
  // ====================================================

  if (
    normalized ==
    "SCROLL:ON"
  ) {

    setScroll(
      true
    );

    return;
  }


  if (
    normalized ==
    "SCROLL:OFF"
  ) {

    setScroll(
      false
    );

    return;
  }


  // ====================================================
  // FALLBACK
  // ====================================================

  // รองรับวิธีเดิม
  // ส่งข้อความตรง ๆ โดยไม่มี TEXT:
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
  // DEBUG STATE
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


  sprite.createSprite(
    tft.width(),
    tft.height()
  );


  sprite.setTextSize(
    textSize
  );


  sprite.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );


  sprite.setTextWrap(
    false
  );


  textWidth =
    sprite.textWidth(
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
    "HairClip ready!"
  );

  Serial.println(
    "Waiting for connection..."
  );
}


// ======================================================
// LOOP
// ======================================================

void loop() {

  // ====================================================
  // PHYSICAL POWER BUTTON
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
  // READ BLE COMMAND
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


  // ====================================================
  // EXECUTE COMMAND
  // ====================================================

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

    sprite.fillSprite(
      TFT_BLACK
    );


    int textHeight =
      8 * textSize;


    int y =
      (
        sprite.height()
        - textHeight
      )
      / 2;


    sprite.setCursor(
      x,
      y
    );


    sprite.print(
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