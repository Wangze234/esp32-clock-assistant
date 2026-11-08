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
#include "esp32-hal-periman.h"

TFT_eSPI tft = TFT_eSPI();

// 打印某个 GPIO 当前被哪个外设占用，用来确认 TFT_eSPI 真的把引脚绑到 SPI 上了
static const char* busName(uint8_t pin) {
  switch (perimanGetPinBusType(pin)) {
    case ESP32_BUS_TYPE_GPIO:            return "GPIO";
    case ESP32_BUS_TYPE_SPI_MASTER_SCK:  return "SPI_SCK";
    case ESP32_BUS_TYPE_SPI_MASTER_MISO: return "SPI_MISO";
    case ESP32_BUS_TYPE_SPI_MASTER_MOSI: return "SPI_MOSI";
    case ESP32_BUS_TYPE_SPI_MASTER_SS:   return "SPI_SS";
    default:                             return "other/free";
  }
}

static void printPinMap() {
  struct { uint8_t pin; const char* name; } pins[] = {
    {TFT_SCLK, "SCLK"}, {TFT_MOSI, "MOSI"}, {TFT_MISO, "MISO"},
    {TFT_CS,   "CS"},   {TFT_DC,   "DC"},
#ifdef TFT_RST
    {TFT_RST,  "RST"},
#endif
    {TFT_BL,   "BL"},   {TOUCH_CS, "T_CS"},
  };
  Serial.println("--- pin -> peripheral ---");
  for (auto &p : pins) {
    Serial.printf("  GPIO%-2u %-5s : %s\n", p.pin, p.name, busName(p.pin));
  }
  Serial.println("-------------------------");
}

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

// 记录各次全屏填充耗时，供心跳行打印（串口中途连上也能看到）
static char g_fillSummary[96] = "";
// 最近一次触摸原始读数，心跳行里一起打印，方便判断触摸线是否通
static uint16_t g_touchX = 0, g_touchY = 0;

// ---------- 触摸坐标标定（2026-11-08 实测）----------
// 本屏 XPT2046 的 X/Y 与显示**交换 + 双向反相**（setRotation(1) 后 TFT_eSPI 不会自动处理）：
//     屏幕左右 ← raw y，且 raw y 越大越靠**左**
//     屏幕上下 ← raw x，且 raw x 越大越靠**上**
// 依据：先按"raw 大 = 右/下"映射，屏上打点比手指位置 180° 颠倒，故两轴映射区间对调。
static const uint16_t RAW_MIN = 640;    // 触摸面板实际有效范围，约 640..3650
static const uint16_t RAW_MAX = 3650;

static uint16_t rawToScreenX(uint16_t rawY) {
  int v = map(rawY, RAW_MIN, RAW_MAX, tft.width(), 0);    // 反向：raw y 大 = 屏幕左
  return constrain(v, 0, tft.width() - 1);
}
static uint16_t rawToScreenY(uint16_t rawX) {
  int v = map(rawX, RAW_MIN, RAW_MAX, tft.height(), 0);   // 反向：raw x 大 = 屏幕顶
  return constrain(v, 0, tft.height() - 1);
}

// 单色全屏 + 居中文字
static void showSolid(uint16_t color, const char* name) {
  uint32_t t0 = millis();
  tft.fillScreen(color);
  uint32_t dt = millis() - t0;
  tft.setTextDatum(MC_DATUM);
  uint16_t fg = (color == TFT_BLACK) ? TFT_WHITE : TFT_BLACK;
  tft.setTextColor(fg, color);
  tft.drawString(name, tft.width() / 2, tft.height() / 2, 4);
  // 40MHz SPI 下整屏填充应在 20~60ms；几百 ms 说明 SPI 时钟配错了
  Serial.printf("fill: %-6s  %lu ms\n", name, dt);
  snprintf(g_fillSummary + strlen(g_fillSummary),
           sizeof(g_fillSummary) - strlen(g_fillSummary),
           " %c=%lums", name[0], dt);
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
  // 先闪 3 下（0.4s 亮 / 0.4s 灭）：只要屏幕硬件是好的，一定能看到背光闪。
  // 完全没有任何光 → 屏没供电、LED 引脚没接、或 GPIO7 接错。
  Serial.println("backlight: blink x3 (GPIO7) ... watch the screen");
  for (int i = 0; i < 3; i++) {
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    delay(400);
    digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON);
    delay(400);
    Serial.printf("  blink %d\n", i + 1);
  }
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  Serial.println("backlight: ON (GPIO7)");
#endif

  tft.init();
  tft.setRotation(1);            // 横屏 320 x 240
  Serial.printf("resolution: %d x %d\n", tft.width(), tft.height());
  printPinMap();

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
  tft.drawString("press: dot marks the spot", tft.width() / 2, 195, 2);
  // 边框：用来判断打点是否落在按压位置
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_DARKGREY);

  // 触摸指示点
  tft.fillCircle(160, 222, 4, TFT_DARKGREY);

  Serial.println("display test done, touch test running...");
  Serial.println("press the screen to see raw coordinates");
}

void loop() {
#ifdef TOUCH_CS
  // 注意：TFT_eSPI 的 getTouchRaw() 永远返回 true（见 Touch.cpp 注释），
  // 不能拿返回值判断"按下"。判断"按下"要看压力值 getTouchRawZ()：
  //   没接触时 Z 很小（几十），手指/指甲压下去 Z 会跳到几百以上。
  static uint16_t lastX = 0xFFFF, lastY = 0xFFFF;
  static uint32_t lastPrint = 0;
  // 最近 5 秒内触摸读数的变化范围：范围大 = XPT2046 在真正应答，
  // 说明 SPI 的 SCK/MOSI 这两根线是通的（触摸和显示共用它们）
  static uint16_t minX = 0xFFFF, maxX = 0, minY = 0xFFFF, maxY = 0;
  static uint32_t winStart = 0;
  static bool     wasDown = false;
  static uint16_t downX = 0, downY = 0, downZ = 0;
  uint16_t x = 0, y = 0, z = 0;

  tft.getTouchRaw(&x, &y);
  z = tft.getTouchRawZ();
  g_touchX = x; g_touchY = y;

  const uint16_t Z_THRESHOLD = 400;   // 经验值：>400 认为真的按下了
  bool down = (z > Z_THRESHOLD);

  if (down) {
    downX = x; downY = y; downZ = z;
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
  }

  if (down && !wasDown) {
    uint16_t sx = rawToScreenX(y), sy = rawToScreenY(x);
    Serial.printf("TOUCH DOWN: raw x=%u y=%u z=%u  -> screen %u,%u\n", x, y, z, sx, sy);
    tft.fillCircle(sx, sy, 4, TFT_RED);      // 在按下处打点，肉眼验证映射对不对
  } else if (!down && wasDown) {
    Serial.printf("TOUCH UP:   last  x=%u  y=%u  z=%u\n", downX, downY, downZ);
  } else if (down && millis() - lastPrint > 300) {
    // 按住不放时每 300ms 报一次，方便看手指移动
    lastPrint = millis();
    uint16_t sx = rawToScreenX(y), sy = rawToScreenY(x);
    Serial.printf("TOUCH MOVE: raw x=%u y=%u z=%u  -> screen %u,%u\n", x, y, z, sx, sy);
    tft.drawCircle(sx, sy, 6, TFT_YELLOW);
  }
  wasDown = down;

  // 每 5 秒汇报一次"真正按下时"的坐标范围，然后清空重新统计
  if (millis() - winStart > 5000) {
    winStart = millis();
    if (minX != 0xFFFF) {           // 这 5 秒内至少按下过一次
      Serial.printf("[touch] 5s pressed range: x %u..%u (span %u)  y %u..%u (span %u)\n",
                    minX, maxX, maxX - minX, minY, maxY, maxY - minY);
    }
    minX = minY = 0xFFFF; maxX = maxY = 0;
  }
#endif

  // 心跳，方便串口确认程序还在跑（不接串口监视器也不影响）
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 5000) {
    lastBeat = millis();
    Serial.printf("[alive] uptime %lu s |%s | touch raw x=%u y=%u\n",
                  millis() / 1000, g_fillSummary, g_touchX, g_touchY);
  }

  delay(30);
}
