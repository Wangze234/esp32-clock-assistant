/*
 * ============================================================
 * 阶段 2：WiFi + NTP 时钟（屏幕 + 串口双输出）
 * 主控：YD-ESP32-S3 N16R8
 * ============================================================
 *
 * 做四件事：
 *   1. 连 WiFi（断网自动重连，**全过程不阻塞**）
 *   2. 从 NTP 取北京时间
 *   3. 串口每秒打印一行
 *   4. 屏幕上画时钟（大号时间 + 日期星期 + 状态栏）
 *
 * 依赖：
 *   - Arduino 内置 WiFi / time
 *   - TFT_eSPI，且必须用 firmware/stage1_display/User_Setup.h 覆盖库里同名文件
 *
 * 配置：把 ../common/secrets.example.h 复制到本目录并改名 secrets.h，
 *       填上真实 WiFi 名称和密码。secrets.h 已被 .gitignore 排除，不要提交。
 *
 * ⚠️ 关键设计：早期版本用 while 循环等 WiFi 连上（每次重试卡 15 秒），
 *    断网时表盘会定住 15 秒不动。现在改成状态机：只"发起"连接，
 *    结果在 loop() 里轮询，表盘永远每秒都在跳。
 * ============================================================
 */

#include <WiFi.h>
#include <time.h>
#include <TFT_eSPI.h>

#include "secrets.h"

static TFT_eSPI tft = TFT_eSPI();

// ---------- 时区 ----------
// 中国标准时间 = UTC+8，无夏令时
static const long GMT_OFFSET_SEC = 8 * 3600;
static const int  DAYLIGHT_OFFSET_SEC = 0;

// NTP 服务器：优先国内，其次公共池
static const char* NTP1 = "ntp.aliyun.com";
static const char* NTP2 = "cn.pool.ntp.org";
static const char* NTP3 = "pool.ntp.org";

// 2023-11-14 之后的时间戳。小于它说明还没对时成功
static const time_t TIME_VALID_SINCE = 1700000000;

// ---------- 配色（参照 DUKA 那种 LCD 表：浅灰底 + 深色字）----------
// 想改回深灰底白字：BG_COLOR = 0x4208、FG_COLOR = TFT_WHITE、FG_DIM = TFT_LIGHTGREY
static const uint16_t BG_COLOR   = 0xC618;         // 背景：浅灰
static const uint16_t FG_COLOR   = TFT_BLACK;      // 大号时间
static const uint16_t FG_DIM     = 0x4208;         // 顶栏日期：深灰
static const uint16_t LINE_COLOR = 0x8410;         // 分隔线

// ---------- 布局（横屏 320x240）----------
//   0  ┌──────────────────────────────┐
//      │ 日期            WiFi 图标     │ 顶栏 40px
//   40 ├──────────────────────────────┤
//      │        大号时间 HH:MM         │ 字体 8 = 75px 高，5 个字符共 265px 宽
//  170 │  （留给天气/温度的区域）        │ 阶段 3、4 用
//  240 └──────────────────────────────┘
static const int TOP_BAR_H = 40;    // 顶栏高度
static const int CLOCK_Y   = 105;   // 大号时间的中心 y（顶栏与底部预留区之间）

static const char* WEEKDAY_CN[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
// TFT_eSPI 内置字体只支持 ASCII，屏幕上用英文缩写；中文要等阶段 5 的 LVGL 字库
static const char* WEEKDAY_EN[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// ---------- WiFi / NTP 状态机 ----------
static const uint32_t WIFI_RETRY_TIMEOUT_MS = 15000;  // 一次连接尝试最多等这么久
static const uint32_t WIFI_RETRY_GAP_MS     = 5000;   // 两次尝试之间的间隔
static const uint32_t STATUS_REFRESH_MS     = 10000;  // 状态栏刷新周期

static bool     g_wasConnected = false;
static bool     g_timeWasValid = false;
static uint32_t g_attemptAt    = 0;   // 0 = 当前没有正在进行的连接尝试
static uint32_t g_nextTryAt    = 0;
static uint32_t g_bootAt       = 0;
static bool     g_statusDirty  = true;

// ============================================================
// WiFi：只发起连接，不等结果
// ============================================================
static void wifiBeginConnect() {
  Serial.printf("WiFi: connecting to \"%s\"\n", WIFI_SSID);
  // 先清掉上一次尝试残留的状态，否则会打印
  // "sta is connecting, cannot set config" 且一直连不上
  WiFi.disconnect(false, false);
  delay(5);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  g_attemptAt = millis();
  g_statusDirty = true;
}

// 每次 loop 调一次，单次执行时间 < 1ms
static void wifiService() {
  if (WiFi.status() == WL_CONNECTED) {
    g_attemptAt = 0;
    if (!g_wasConnected) {
      g_wasConnected = true;
      Serial.printf("WiFi: OK  IP=%s  RSSI=%d dBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      // 连上就请求一次对时（configTime 本身不阻塞，SNTP 在后台跑）
      configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP1, NTP2, NTP3);
      Serial.println(g_timeWasValid ? "NTP: re-syncing after reconnect ..."
                                    : "NTP: requesting time ...");
      g_statusDirty = true;
    }
    return;
  }

  // ---- 未连接 ----
  if (g_wasConnected) {
    g_wasConnected = false;
    Serial.printf("[wifi] lost (status %d)\n", WiFi.status());
    g_statusDirty = true;
  }

  uint32_t now = millis();
  if (g_attemptAt == 0) {
    if ((int32_t)(now - g_nextTryAt) >= 0) wifiBeginConnect();
  } else if (now - g_attemptAt >= WIFI_RETRY_TIMEOUT_MS) {
    Serial.println("WiFi: attempt timeout, will retry");
    g_attemptAt = 0;
    g_nextTryAt = now + WIFI_RETRY_GAP_MS;
  }
}

// ============================================================
// 自绘方角数码管（不用库里的 7 段字体，笔画全是矩形，更"方"）
// ============================================================
// 段位：bit0=A(顶) bit1=B(右上) bit2=C(右下) bit3=D(底) bit4=E(左下) bit5=F(左上) bit6=G(中)
static const uint8_t SEG_MAP[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// 数字/冒号的尺寸，想改大小只动这四个数
static const int SEG_W      = 44;   // 单字宽
static const int SEG_H      = 88;   // 单字高
static const int SEG_T      = 12;   // 笔画粗细
static const int SEG_GAP    = 10;   // 字间距
static const int SEG_COLON  = 16;   // 冒号占宽

// 每一笔都是圆角矩形：只要一点点圆角，整体仍然是方的（阶段 3 定的样式）
static const int SEG_R = 4;         // 圆角半径，0 = 完全直角

static void segRect(int x, int y, int w, int h, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, SEG_R, color);
}

static void drawSegDigit(char c, int x, int y, int w, int h, int t, uint16_t color) {
  if (c < '0' || c > '9') return;
  uint8_t m = SEG_MAP[c - '0'];
  int half = h / 2;
  if (m & 0x01) segRect(x, y, w, t, color);                    // A 顶
  if (m & 0x02) segRect(x + w - t, y, t, half, color);         // B 右上
  if (m & 0x04) segRect(x + w - t, y + half, t, half, color);  // C 右下
  if (m & 0x08) segRect(x, y + h - t, w, t, color);            // D 底
  if (m & 0x10) segRect(x, y + half, t, half, color);          // E 左下
  if (m & 0x20) segRect(x, y, t, half, color);                 // F 左上
  if (m & 0x40) segRect(x, y + half - t / 2, w, t, color);     // G 中
}

static int segStringWidth(const char* s) {
  int total = 0;
  for (const char* p = s; *p; p++) total += (*p == ':') ? SEG_COLON : (SEG_W + SEG_GAP);
  return total - SEG_GAP;
}

// 画 "HH:MM"；每次先清掉整个时间区域，避免残影
static void drawTimeBig(const char* s) {
  int total = segStringWidth(s);
  int x0 = (tft.width() - total) / 2;
  int y  = CLOCK_Y - SEG_H / 2;

  tft.fillRect(x0 - 4, y - 6, total + 8, SEG_H + 12, BG_COLOR);

  int x = x0;
  for (const char* p = s; *p; p++) {
    if (*p == ':') {
      int cx = x + SEG_COLON / 2 - 4;
      for (int k = 0; k < 2; k++) {
        int cy = y + (k ? SEG_H * 62 / 100 : SEG_H * 28 / 100);
        tft.fillRoundRect(cx, cy, 8, 8, 2, FG_COLOR);
      }
      x += SEG_COLON;
    } else {
      drawSegDigit(*p, x, y, SEG_W, SEG_H, SEG_T, FG_COLOR);
      x += SEG_W + SEG_GAP;
    }
  }
}

// ============================================================
// 屏幕：初始化 + 静态框架
// ============================================================
static void displayInit() {
#ifdef TFT_BL
  // 背光闪 3 下：确认程序和背光控制都活着
  pinMode(TFT_BL, OUTPUT);
  Serial.println("backlight: blink x3 (GPIO7)");
  for (int i = 0; i < 3; i++) {
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    delay(300);
    digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON);
    delay(300);
  }
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif

  tft.init();
  tft.setRotation(1);            // 横屏 320 x 240
  tft.fillScreen(BG_COLOR);
  Serial.printf("display: %d x %d\n", tft.width(), tft.height());

  // 顶栏分隔线（参考图那种"状态栏 + 线 + 大数字"的结构）
  tft.drawFastHLine(0, TOP_BAR_H, tft.width(), LINE_COLOR);

  // 没对上时之前：时间区留空，不显示假数字
}

// 右上角 WiFi 图标：4 根信号柱，信号越好亮的根数越多；断网时打红叉。
// -2 = 还没画过；-1 = 未连接；1..4 = 信号等级
static int8_t g_lastBars = -2;

static int8_t wifiBars() {
  if (WiFi.status() != WL_CONNECTED) return -1;
  int r = WiFi.RSSI();
  if (r >= -60) return 4;
  if (r >= -70) return 3;
  if (r >= -80) return 2;
  return 1;
}

static void drawWifiIcon() {
  int8_t bars = wifiBars();
  if (bars == g_lastBars) return;        // 状态没变就不重画，避免闪烁
  g_lastBars = bars;

  // 小巧版：4 根柱，总宽 18px、最高 9px，贴在右上角
  const int x0 = 294, base = 30, w = 3, gap = 2;
  const int h[4] = {3, 5, 7, 9};
  tft.fillRect(288, 16, 26, 20, BG_COLOR);          // 先清掉旧图标

  for (int i = 0; i < 4; i++) {
    uint16_t c = (bars > 0 && i < bars) ? FG_COLOR : 0x9CF3;   // 未点亮的柱子：浅一些
    tft.fillRect(x0 + i * (w + gap), base - h[i], w, h[i], c);
  }
  if (bars < 0) {                                   // 未连接：红色小叉
    tft.drawLine(292, 18, 312, 30, TFT_RED);
    tft.drawLine(312, 18, 292, 30, TFT_RED);
  }
  g_statusDirty = false;
}

// ============================================================
// 时钟：串口 + 屏幕，只在"秒数变化"时执行一次
// ============================================================
static void updateClock() {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);

  // 用 millis() 做 1 秒定时器会和 RTC 的秒边界不同步，
  // 会出现同一秒打印两次、下一秒漏掉。改成按秒数去重。
  static int lastSec = -1;
  if (t.tm_sec == lastSec) return;
  lastSec = t.tm_sec;

  char hms[16], date[32];
  strftime(hms, sizeof(hms), "%H:%M:%S", &t);
  strftime(date, sizeof(date), "%Y-%m-%d", &t);

  // ----- 顶栏左侧：日期 + 英文星期（只在日期变化时重画）-----
  static char lastTop[20] = "";
  char top[20];
  strftime(top, sizeof(top), "%m/%d  %a", &t);     // 例：11/08  Sun
  if (strcmp(top, lastTop) != 0) {
    strcpy(lastTop, top);
    tft.fillRect(0, 0, 180, TOP_BAR_H - 1, BG_COLOR);   // 清掉旧日期
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(FG_DIM, BG_COLOR);
    tft.drawString(top, 10, TOP_BAR_H / 2, 2);          // 字体 2（16px），比原来小一号
  }

  // ----- 屏幕：大号时间 HH:MM（自绘方角数码管）-----
  // 8 位 HH:MM:SS 会超宽，所以大字只显示 HH:MM，秒用小字放右侧
  char hm[8], ss[4];
  strftime(hm, sizeof(hm), "%H:%M", &t);
  strftime(ss, sizeof(ss), "%S", &t);

  drawTimeBig(hm);

  // 秒：右侧小字，与大数字底部对齐
  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(FG_DIM, BG_COLOR);
  tft.drawString(ss, 316, CLOCK_Y + SEG_H / 2 - 2, 2);

  // ----- 串口 -----
  // 断网时 WiFi.RSSI() 返回 0，直接打 "0 dBm" 会误导，改成 "--"
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("%s %s  %s  RSSI %d dBm  heap %u KB\n",
                  date, hms, WEEKDAY_CN[t.tm_wday % 7], WiFi.RSSI(),
                  (unsigned)(ESP.getFreeHeap() / 1024));
  } else {
    Serial.printf("%s %s  %s  WiFi --      heap %u KB\n",
                  date, hms, WEEKDAY_CN[t.tm_wday % 7],
                  (unsigned)(ESP.getFreeHeap() / 1024));
  }
}

void setup() {
  Serial.begin(115200);
  g_bootAt = millis();
  delay(800);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" Stage 2 : WiFi + NTP clock");
  Serial.println("========================================");

  displayInit();

  Serial.println("WiFi: starting (non-blocking)");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // 关掉省电，否则秒跳不均匀
  wifiBeginConnect();            // 只发起，结果在 loop() 里轮询
}

void loop() {
  // ---------- 1. WiFi / NTP 状态机（永不阻塞）----------
  wifiService();

  // ---------- 2. 时间刚对上的那一刻，打印一次耗时 ----------
  bool valid = (time(nullptr) >= TIME_VALID_SINCE);
  if (valid && !g_timeWasValid) {
    Serial.printf("NTP: OK (took %lu ms since boot)\n", millis() - g_bootAt);
    g_statusDirty = true;
  }
  g_timeWasValid = valid;

  // ---------- 3. 右上角 WiFi 图标按需刷新（断网/重连/信号档位变化时）----------
  static uint32_t lastStatus = 0;
  if (g_statusDirty || millis() - lastStatus > STATUS_REFRESH_MS) {
    lastStatus = millis();
    drawWifiIcon();
  }

  // ---------- 4. 每秒刷一次表盘（updateClock 内部按秒去重）----------
  if (valid) {
    updateClock();
  } else {
    // 只在"已经连上 WiFi 但还没对时"时才抱怨，刚开机那几秒不刷屏
    static uint32_t lastWarn = 0;
    if (WiFi.status() == WL_CONNECTED && millis() - lastWarn > 5000) {
      lastWarn = millis();
      Serial.println("[time] not synced yet");
    }
  }

  delay(10);
}
