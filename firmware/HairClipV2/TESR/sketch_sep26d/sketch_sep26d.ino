#include <TFT_eSPI.h>

#define TFT_BL 4

#define BATTERY_ADC_EN 14
#define BATTERY_ADC_PIN 34

TFT_eSPI tft = TFT_eSPI();


// ======================================================
// READ BATTERY VOLTAGE
// ======================================================

float readBatteryVoltage() {

  const int samples = 20;

  uint32_t totalMilliVolts = 0;

  for (int i = 0; i < samples; i++) {

    totalMilliVolts +=
      analogReadMilliVolts(
        BATTERY_ADC_PIN
      );

    delay(5);
  }


  float averageMilliVolts =
    totalMilliVolts /
    (float)samples;


  // T-Display ใช้ voltage divider ประมาณ 1:2
  float batteryVoltage =
    (
      averageMilliVolts *
      2.0
    )
    /
    1000.0;


  return batteryVoltage;
}


// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);


  // ----------------------------------------------------
  // DISPLAY
  // ----------------------------------------------------

  pinMode(
    TFT_BL,
    OUTPUT
  );

  digitalWrite(
    TFT_BL,
    HIGH
  );


  tft.init();

  tft.setRotation(1);

  tft.fillScreen(
    TFT_BLACK
  );


  tft.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );


  // ----------------------------------------------------
  // BATTERY ADC
  // ----------------------------------------------------

  pinMode(
  BATTERY_ADC_EN,
  OUTPUT
);

// เปิดวงจรวัดแบตค้างไว้
digitalWrite(
  BATTERY_ADC_EN,
  HIGH
);


  pinMode(
    BATTERY_ADC_PIN,
    INPUT
  );


  analogSetPinAttenuation(
    BATTERY_ADC_PIN,
    ADC_11db
  );


  delay(500);
}


// ======================================================
// LOOP
// ======================================================

void loop() {

  float voltage =
    readBatteryVoltage();


  // ล้างเฉพาะส่วนข้อมูล
  tft.fillScreen(
    TFT_BLACK
  );


  // ----------------------------------------------------
  // TITLE
  // ----------------------------------------------------

  tft.setTextDatum(
    MC_DATUM
  );


  tft.setTextSize(2);


  tft.drawString(
    "BATTERY",
    120,
    35
  );


  // ----------------------------------------------------
  // VOLTAGE
  // ----------------------------------------------------

  String voltageText =
    String(
      voltage,
      2
    )
    + " V";


  tft.setTextSize(3);


  tft.drawString(
    voltageText,
    120,
    75
  );


  // ----------------------------------------------------
  // SERIAL
  // ----------------------------------------------------

  Serial.print(
    "Battery: "
  );

  Serial.print(
    voltage,
    3
  );

  Serial.println(
    " V"
  );


  delay(1000);
}