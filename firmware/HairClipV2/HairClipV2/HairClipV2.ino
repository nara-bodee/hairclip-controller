#include <TFT_eSPI.h>
#include <esp_sleep.h>
#include <Preferences.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ======================================================
// DISPLAY
// ======================================================

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

#define TFT_BL 4
#define POWER_BUTTON 35

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

String pendingCommand = "";
bool newCommandReady = false;


// ======================================================
// BLE CHARACTERISTIC CALLBACK
// ======================================================

class CommandCallbacks : public BLECharacteristicCallbacks {

  // ----------------------------------------------------
  // รับคำสั่งจากมือถือ / Web Controller
  // ----------------------------------------------------

  void onWrite(BLECharacteristic *pCharacteristic) override {

    String value = pCharacteristic->getValue();

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

      xSemaphoreGive(commandMutex);
    }
  }


  // ----------------------------------------------------
  // ส่ง State ปัจจุบันกลับไปหา Web Controller
  // ----------------------------------------------------

  void onRead(BLECharacteristic *pCharacteristic) override {

    String state =
      String(scrollEnabled ? "1" : "0")
      + ","
      + String(displayEnabled ? "1" : "0")
      + "\n"
      + message;

    pCharacteristic->setValue(
      state.c_str()
    );

    Serial.println("State requested:");
    Serial.println(state);
  }
};


// ======================================================
// BLE SERVER CALLBACK
// ======================================================

class ServerCallbacks : public BLEServerCallbacks {

  void onConnect(BLEServer *pServer) override {

    Serial.println("Phone connected!");
  }


  void onDisconnect(BLEServer *pServer) override {

    Serial.println("Phone disconnected!");

    // เปิด Advertising ใหม่
    // เพื่อให้มือถือกลับมา Connect ได้อีก
    pServer->getAdvertising()->start();
  }
};


// ======================================================
// DEEP SLEEP
// ======================================================

void goToSleep() {

  Serial.println("Going to deep sleep...");
  Serial.flush();

  // ปิด Backlight
  digitalWrite(
    TFT_BL,
    LOW
  );

  // รอจนผู้ใช้ปล่อยปุ่ม
  while (
    digitalRead(POWER_BUTTON) == LOW
  ) {

    delay(10);
  }

  delay(200);

  // GPIO35 ใช้เป็น Wake source
  esp_sleep_enable_ext0_wakeup(
    GPIO_NUM_35,
    0
  );

  esp_deep_sleep_start();
}


// ======================================================
// COMMAND HANDLER
// ======================================================

void handleCommand(String command) {

  command.trim();


  // ====================================================
  // TEXT
  // ====================================================

  if (
    command.startsWith("TEXT:")
  ) {

    String newMessage =
      command.substring(5);

    newMessage.trim();

    if (
      newMessage.length() > 0
    ) {

      message = newMessage;

      // บันทึกข้อความลง Flash
      preferences.putString(
        "message",
        message
      );

      // คำนวณความกว้างใหม่
      textWidth =
        sprite.textWidth(message);

      // ให้ข้อความใหม่เริ่มจากขวา
      x = sprite.width();

      Serial.print(
        "New text: "
      );

      Serial.println(
        message
      );
    }

    return;
  }


  // ====================================================
  // DISPLAY ON
  // ====================================================

  if (
    command == "DISPLAY:ON"
  ) {

    displayEnabled = true;

    preferences.putBool(
      "display",
      true
    );

    digitalWrite(
      TFT_BL,
      HIGH
    );

    Serial.println(
      "Display ON"
    );

    return;
  }


  // ====================================================
  // DISPLAY OFF
  // ====================================================

  if (
    command == "DISPLAY:OFF"
  ) {

    displayEnabled = false;

    preferences.putBool(
      "display",
      false
    );

    digitalWrite(
      TFT_BL,
      LOW
    );

    Serial.println(
      "Display OFF"
    );

    return;
  }


  // ====================================================
  // SCROLL ON
  // ====================================================

  if (
    command == "SCROLL:ON"
  ) {

    scrollEnabled = true;

    preferences.putBool(
      "scroll",
      true
    );

    // เริ่มจากทางขวาใหม่
    x = sprite.width();

    Serial.println(
      "Scroll ON"
    );

    return;
  }


  // ====================================================
  // SCROLL OFF
  // ====================================================

  if (
    command == "SCROLL:OFF"
  ) {

    scrollEnabled = false;

    preferences.putBool(
      "scroll",
      false
    );

    // จัดข้อความให้อยู่ตรงกลาง
    x =
      (sprite.width() - textWidth)
      / 2;

    Serial.println(
      "Scroll OFF"
    );

    return;
  }


  // ====================================================
  // FALLBACK
  // ====================================================

  // รองรับการส่งข้อความแบบเก่า
  // เช่นส่ง "HELLO" มาโดยไม่มี TEXT:
  message = command;

  preferences.putString(
    "message",
    message
  );

  textWidth =
    sprite.textWidth(message);

  x = sprite.width();

  Serial.print(
    "Raw text: "
  );

  Serial.println(
    message
  );
}


// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

  delay(200);


  // ====================================================
  // POWER BUTTON
  // ====================================================

  pinMode(
    POWER_BUTTON,
    INPUT
  );


  // ====================================================
  // NVS STORAGE
  // ====================================================

  preferences.begin(
    "hairclip",
    false
  );


  // โหลด Message ล่าสุด
  message =
    preferences.getString(
      "message",
      "HELLO WORLD!"
    );


  // โหลดสถานะ Display ล่าสุด
  displayEnabled =
    preferences.getBool(
      "display",
      true
    );


  // โหลดสถานะ Scroll ล่าสุด
  scrollEnabled =
    preferences.getBool(
      "scroll",
      true
    );


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


  // สร้าง Sprite เท่าขนาดจอ
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


  // ไม่ให้ตัดข้อความขึ้นบรรทัดใหม่
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
  // COMMAND MUTEX
  // ====================================================

  commandMutex =
    xSemaphoreCreateMutex();


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
  // READ PENDING BLE COMMAND
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

    // ล้าง Sprite ใน RAM
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


    // ส่ง Frame ขึ้นจอ
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

      x -= scrollSpeed;


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