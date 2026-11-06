/*
 * ============================================================
 * 阶段 1：MSP2807 屏幕自检
 * 屏幕：2.8" 320x240 ILI9341 + XPT2046 触摸
 * 主控：YD-ESP32-S3 N16R8
 * ============================================================
 *
 * 烧录后现象：
 *   1. 红 → 绿 → 蓝 → 白 → 黑 全屏切换
 *   2. 8 色竖条
 *   3. 停在信息页，显示 "Stage 1 OK"
 *   4. 串口打印分辨率和触摸原始坐标
 *
 * 依赖：
 *   - TFT_eSPI 库（Bodmer）
 *   - 用本目录的 User_Setup.h 覆盖库里的同名文件
 * ============================================================
 */

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// 画 8 色竖条
static void drawColorBars() {
  const uint16_t colors[8] = {
    TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN,
    TFT_CYAN, TFT_BLUE, TFT_MAGENTA, TFT_WHITE
  };
  const char* names[8] = {
    "RED", "ORANGE", "YELLOW", "GREEN",
    "CYAN", "BLUE", "MAGENTA", "WHITE"
  };

  int barW = tft.width() / 8;
  for (int i = 0; i < 8; i++) {
    tft.fillRect(i * barW, 0, barW, tft.height(), colors[i]);
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(TFT_BLACK, colors[i]);
    tft.drawString(names[i], i * barW + barW / 2, tft.height() - 6, 2);
  }
}

// 单色全屏 + 居中文字
static void showSolid(uint16_t color, const char* name) {
  tft.fillScreen(color);
  tft.setTextDatum(MC_DATUM);
  uint16_t fg = (color == TFT_BLACK) ? TFT_WHITE : TFT_BLACK;
  tft.setTextColor(fg, color);
  tft.drawString(name, tft.width() / 2, tft.height() / 2, 4);
  Serial.printf("fill: %s\n", name);
  delay(700);
}

void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" Stage 1 : MSP2807 / ILI9341 self test");
  Serial.println("========================================");

  // 背光先打开，便于排查"黑屏但程序在跑"
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  Serial.println("backlight: ON (GPIO7)");
#endif

  tft.init();
  tft.setRotation(1);            // 横屏 320 x 240
  Serial.printf("resolution: %d x %d\n", tft.width(), tft.height());

  // 1. 纯色测试
  showSolid(TFT_RED,   "RED");
  showSolid(TFT_GREEN, "GREEN");
  showSolid(TFT_BLUE,  "BLUE");
  showSolid(TFT_WHITE, "WHITE");
  showSolid(TFT_BLACK, "BLACK");

  // 2. 色条测试
  drawColorBars();
  Serial.println("color bars drawn");
  delay(1800);

  // 3. 信息页
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("MSP2807 / ILI9341", tft.width() / 2, 42, 4);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("320 x 240  4-wire SPI", tft.width() / 2, 82, 2);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Stage 1 OK", tft.width() / 2, 138, 4);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("touch the screen", tft.width() / 2, 195, 2);

  // 触摸指示点
  tft.fillCircle(160, 222, 4, TFT_DARKGREY);

  Serial.println("display test done, touch test running...");
  Serial.println("press the screen to see raw coordinates");
}

void loop() {
#ifdef TOUCH_CS
  static bool lastPressed = false;
  uint16_t x = 0, y = 0;

  bool pressed = tft.getTouchRaw(&x, &y);

  if (pressed && !lastPressed) {
    Serial.printf("TOUCH raw: x=%u  y=%u\n", x, y);
    tft.fillCircle(160, 222, 4, TFT_RED);
  } else if (!pressed && lastPressed) {
    Serial.println("TOUCH released");
    tft.fillCircle(160, 222, 4, TFT_DARKGREY);
  }
  lastPressed = pressed;
#endif

  delay(30);
}
