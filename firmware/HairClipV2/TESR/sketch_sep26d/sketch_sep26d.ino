#include <TFT_eSPI.h>
#include <math.h>

#define TFT_BL 4

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);


// ======================================================
// HEART
// ======================================================

void drawHeart(int x, int y, uint16_t color) {

  sprite.fillCircle(
    x - 6,
    y - 4,
    7,
    color
  );

  sprite.fillCircle(
    x + 6,
    y - 4,
    7,
    color
  );

  sprite.fillTriangle(
    x - 13,
    y - 2,
    x + 13,
    y - 2,
    x,
    y + 16,
    color
  );
}


// ======================================================
// STAR
// ======================================================

void drawStar(int cx, int cy, uint16_t color) {

  const int points = 10;

  int px[points];
  int py[points];

  for (int i = 0; i < points; i++) {

    float angle =
      -PI / 2
      + i * PI / 5;

    float radius =
      (i % 2 == 0)
        ? 15
        : 7;

    px[i] =
      cx
      + cos(angle) * radius;

    py[i] =
      cy
      + sin(angle) * radius;
  }

  for (int i = 0; i < points; i++) {

    int next =
      (i + 1) % points;

    sprite.drawLine(
      px[i],
      py[i],
      px[next],
      py[next],
      color
    );
  }
}


// ======================================================
// SMILE
// ======================================================

void drawSmile(int x, int y, uint16_t color) {

  sprite.drawCircle(
    x,
    y,
    15,
    color
  );

  sprite.fillCircle(
    x - 5,
    y - 4,
    2,
    color
  );

  sprite.fillCircle(
    x + 5,
    y - 4,
    2,
    color
  );

  // รอยยิ้ม
  sprite.drawLine(
    x - 7,
    y + 4,
    x - 3,
    y + 8,
    color
  );

  sprite.drawLine(
    x - 3,
    y + 8,
    x + 3,
    y + 8,
    color
  );

  sprite.drawLine(
    x + 3,
    y + 8,
    x + 7,
    y + 4,
    color
  );
}


// ======================================================
// SUN
// ======================================================

void drawSun(int x, int y, uint16_t color) {

  sprite.drawCircle(
    x,
    y,
    8,
    color
  );

  for (int i = 0; i < 8; i++) {

    float angle =
      i * PI / 4;

    int x1 =
      x
      + cos(angle) * 12;

    int y1 =
      y
      + sin(angle) * 12;

    int x2 =
      x
      + cos(angle) * 17;

    int y2 =
      y
      + sin(angle) * 17;

    sprite.drawLine(
      x1,
      y1,
      x2,
      y2,
      color
    );
  }
}


// ======================================================
// MOON
// ======================================================

void drawMoon(int x, int y, uint16_t color) {

  sprite.fillCircle(
    x,
    y,
    15,
    color
  );

  // วงดำทับเพื่อสร้างพระจันทร์เสี้ยว
  sprite.fillCircle(
    x + 7,
    y - 4,
    14,
    TFT_BLACK
  );
}


// ======================================================
// MUSIC
// ======================================================

void drawMusic(int x, int y, uint16_t color) {

  sprite.fillCircle(
    x - 6,
    y + 10,
    4,
    color
  );

  sprite.fillCircle(
    x + 9,
    y + 6,
    4,
    color
  );

  sprite.drawLine(
    x - 2,
    y + 10,
    x - 2,
    y - 12,
    color
  );

  sprite.drawLine(
    x + 13,
    y + 6,
    x + 13,
    y - 16,
    color
  );

  sprite.drawLine(
    x - 2,
    y - 12,
    x + 13,
    y - 16,
    color
  );
}


// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

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

  sprite.setColorDepth(16);

  sprite.createSprite(
    tft.width(),
    tft.height()
  );

  sprite.fillSprite(
    TFT_BLACK
  );


  // แถวบน
  drawHeart(
    40,
    35,
    TFT_WHITE
  );

  drawStar(
    120,
    35,
    TFT_WHITE
  );

  drawSmile(
    200,
    35,
    TFT_WHITE
  );


  // แถวล่าง
  drawSun(
    40,
    95,
    TFT_WHITE
  );

  drawMoon(
    120,
    95,
    TFT_WHITE
  );

  drawMusic(
    200,
    95,
    TFT_WHITE
  );


  sprite.pushSprite(
    0,
    0
  );


  Serial.println(
    "Icon test complete"
  );
}


void loop() {

}