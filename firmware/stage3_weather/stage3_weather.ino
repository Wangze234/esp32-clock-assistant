/*
 * ============================================================
 * 阶段 3：时钟 + 和风天气
 * 主控：YD-ESP32-S3 N16R8
 * ============================================================
 *
 * 在阶段 2 的基础上加：
 *   1. 后台任务定时拉和风天气（现在温度 / 湿度 / 今日最高最低 / 天气图标）
 *   2. 屏幕底部预留区画天气：左边图标、中间当前温度、右边高低温 + 湿度
 *
 * 依赖：
 *   - Arduino 内置 WiFi / time / WiFiClientSecure / HTTPClient
 *   - TFT_eSPI，且必须用 firmware/stage1_display/User_Setup.h 覆盖库里同名文件
 *
 * 配置：把 ../common/secrets.example.h 复制到本目录并改名 secrets.h，填：
 *       WIFI_SSID / WIFI_PASSWORD / QWEATHER_KEY / QWEATHER_HOST / QWEATHER_CITY
 *       secrets.h 已被 .gitignore 排除，不要提交，也不要把内容贴到任何地方。
 *
 * ⚠️ 关键设计（沿用阶段 2 的教训，别改回去）：
 *   - WiFi 用非阻塞状态机，断网时表盘照常每秒跳
 *   - 天气请求放在**独立 FreeRTOS 任务**里：HTTPS 握手要 1~3 秒，
 *     放在 loop() 里会让秒针卡住；任务把结果写进带锁的全局结构体，
 *     loop() 只负责发现"有新数据"然后重画
 * ============================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <TFT_eSPI.h>
#include <miniz.h>          // ROM 里的 tinfl，用来解 gzip（见 wxGet 注释）

#include "secrets.h"

// Arduino 会把自动生成的函数原型插在 include 之后、正文之前，
// 用到自定义结构体做参数时，必须先在这里前置声明，否则 "WeatherData not declared"
struct WeatherData;

static TFT_eSPI tft = TFT_eSPI();

// ---------- 时区 ----------
static const long GMT_OFFSET_SEC = 8 * 3600;
static const int  DAYLIGHT_OFFSET_SEC = 0;

static const char* NTP1 = "ntp.aliyun.com";
static const char* NTP2 = "cn.pool.ntp.org";
static const char* NTP3 = "pool.ntp.org";

static const time_t TIME_VALID_SINCE = 1700000000;

// ---------- 配色（浅灰底 + 深色字）----------
static const uint16_t BG_COLOR   = 0xC618;
static const uint16_t FG_COLOR   = TFT_BLACK;
static const uint16_t FG_DIM     = 0x4208;
static const uint16_t LINE_COLOR = 0x8410;

// ---------- 布局（横屏 320x240）----------
//   0  ┌──────────────────────────────┐
//      │ 日期            WiFi 图标     │ 顶栏 40px
//  40 ├──────────────────────────────┤
//      │        大号时间 HH:MM  :SS    │ 48..152
// 168  ├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─┤
//      │  ☀    8°        12°/2°        │ 天气区 168..240
//      │                  RH 45%       │
//  240 └──────────────────────────────┘
static const int TOP_BAR_H = 40;
static const int CLOCK_Y   = 104;   // 40..168 这段的中心，字高 84
static const int WX_Y_TOP  = 168;

static const char* WEEKDAY_CN[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

// ---------- WiFi / NTP 状态机 ----------
static const uint32_t WIFI_RETRY_TIMEOUT_MS = 15000;
static const uint32_t WIFI_RETRY_GAP_MS     = 5000;
static const uint32_t STATUS_REFRESH_MS     = 10000;

static bool     g_wasConnected = false;
static bool     g_timeWasValid = false;
static uint32_t g_attemptAt    = 0;
static uint32_t g_nextTryAt    = 0;
static uint32_t g_bootAt       = 0;
static bool     g_statusDirty  = true;

// ============================================================
// WiFi：只发起连接，不等结果
// ============================================================
static void wifiBeginConnect() {
  Serial.printf("WiFi: connecting to \"%s\"\n", WIFI_SSID);
  WiFi.disconnect(false, false);
  delay(5);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  g_attemptAt = millis();
  g_statusDirty = true;
}

static void wifiService() {
  if (WiFi.status() == WL_CONNECTED) {
    g_attemptAt = 0;
    if (!g_wasConnected) {
      g_wasConnected = true;
      Serial.printf("WiFi: OK  IP=%s  RSSI=%d dBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP1, NTP2, NTP3);
      Serial.println(g_timeWasValid ? "NTP: re-syncing after reconnect ..."
                                    : "NTP: requesting time ...");
      g_statusDirty = true;
    }
    return;
  }

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
// 自绘方角数码管（轻微斜切）
// ============================================================
// 段位：bit0=A(顶) bit1=B(右上) bit2=C(右下) bit3=D(底) bit4=E(左下) bit5=F(左上) bit6=G(中)
static const uint8_t SEG_MAP[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// 尺寸：HH:MM:SS 六个数字 + 两个冒号必须塞进 320px，所以字比只看 HH:MM 时窄一点
//   总宽 = 6*(SEG_W+SEG_GAP) + 2*SEG_COLON - SEG_GAP = 6*46 + 24 - 6 = 294
static const int SEG_W      = 40;
static const int SEG_H      = 84;
static const int SEG_T      = 11;
static const int SEG_GAP    = 6;
static const int SEG_COLON  = 12;
static const int SEG_R      = 4;    // 笔画圆角：只要一点点，整体仍然是方的

// 每一笔都是圆角矩形，不是斜切
static void segRect(int x, int y, int w, int h, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, SEG_R, color);
}

static void drawSegDigit(char c, int x, int y, int w, int h, int t, uint16_t color) {
  if (c < '0' || c > '9') return;
  uint8_t m = SEG_MAP[c - '0'];
  int half = h / 2;
  if (m & 0x01) segRect(x, y, w, t, color);
  if (m & 0x02) segRect(x + w - t, y, t, half, color);
  if (m & 0x04) segRect(x + w - t, y + half, t, half, color);
  if (m & 0x08) segRect(x, y + h - t, w, t, color);
  if (m & 0x10) segRect(x, y + half, t, half, color);
  if (m & 0x20) segRect(x, y, t, half, color);
  if (m & 0x40) segRect(x, y + half - t / 2, w, t, color);
}

static int segStringWidth(const char* s) {
  int total = 0;
  for (const char* p = s; *p; p++) total += (*p == ':') ? SEG_COLON : (SEG_W + SEG_GAP);
  return total - SEG_GAP;
}

// "1" 只用 B/C 两段，笔画都在字的**右半边**，左半边 29px 是空的。
// 按字框居中时首位是 1 会明显偏左，所以把这块空量补进左边距做光学居中。
static int segInkLeft(const char* s) {
  if (*s == '1') return SEG_W - SEG_T;
  if (*s == ':') return SEG_COLON / 2 - 3;
  return 0;
}

static void drawSegCell(char c, int x, int y) {
  if (c == ':') {
    int cx = x + SEG_COLON / 2 - 3;
    for (int k = 0; k < 2; k++) {
      int cy = y + (k ? SEG_H * 62 / 100 : SEG_H * 28 / 100);
      tft.fillRoundRect(cx, cy, 6, 6, 2, FG_COLOR);
    }
  } else {
    drawSegDigit(c, x, y, SEG_W, SEG_H, SEG_T, FG_COLOR);
  }
}

// 每跳一秒只重画"变了的那一格"，不整条清屏 —— 整条清再重画会有明显闪烁。
// 只有整串位置变化（字宽变了 / 光学居中偏移变了）时才整条重画。
static void drawTimeBig(const char* s) {
  static char lastStr[10] = "";
  static int  lastX0 = -9999;

  int total   = segStringWidth(s);
  int inkLeft = segInkLeft(s);
  int x0 = (tft.width() - total - inkLeft) / 2;
  int y  = CLOCK_Y - SEG_H / 2;

  bool full = (strlen(s) != strlen(lastStr)) || (x0 != lastX0);
  if (full) tft.fillRect(0, y - 6, tft.width(), SEG_H + 12, BG_COLOR);

  int x = x0;
  for (int i = 0; s[i]; i++) {
    int w = (s[i] == ':') ? SEG_COLON : SEG_W;
    if (full || s[i] != lastStr[i]) {
      tft.fillRect(x - 2, y - 2, w + 4, SEG_H + 4, BG_COLOR);   // 只清这一格
      drawSegCell(s[i], x, y);
    }
    x += (s[i] == ':') ? SEG_COLON : (SEG_W + SEG_GAP);
  }

  strcpy(lastStr, s);
  lastX0 = x0;
}

// ============================================================
// 天气数据（后台任务写、loop 读，用临界区保护）
// ============================================================
struct WeatherData {
  bool     valid;
  int      temp;      // 当前温度 ℃
  int      hum;       // 相对湿度 %
  int      tmax;      // 今日最高 ℃
  int      tmin;      // 今日最低 ℃
  int      icon;      // 和风天气图标编号，如 100 / 104 / 305
  char     text[24];  // 天气现象（中文，只打串口）
};

static WeatherData g_wx;
static portMUX_TYPE g_wxMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool g_wxDirty = true;

static const uint32_t WX_REFRESH_MS  = 30UL * 60 * 1000;   // 正常 30 分钟一次
static const uint32_t WX_RETRY_MS    = 60UL * 1000;        // 失败 1 分钟后重试

// 从 JSON 里取 "key":"值" 的字符串值；找不到返回 false
static bool jsonStr(const String& j, const char* key, char* out, size_t n) {
  String pat = String("\"") + key + "\":\"";
  int p = j.indexOf(pat);
  if (p < 0) return false;
  p += pat.length();
  int e = j.indexOf('"', p);
  if (e < 0) return false;
  size_t len = (size_t)(e - p);
  if (len >= n) len = n - 1;
  memcpy(out, j.c_str() + p, len);
  out[len] = 0;
  return true;
}

static int jsonInt(const String& j, const char* key, int def) {
  char buf[16];
  if (!jsonStr(j, key, buf, sizeof(buf))) return def;
  return atoi(buf);
}

// 和风天气的响应**永远是 gzip**（实测就算发了 Accept-Encoding: identity 也照样压缩），
// ESP32 的 HTTPClient 不会自动解压，所以在这里手动解。
//
// ROM 里的 tinfl 只认 raw deflate / zlib，不认 gzip 外壳，所以要先跳过
// gzip 的 10 字节固定头 + 可选扩展字段（FEXTRA/FNAME/FCOMMENT/FHCRC），
// 尾部再掐掉 8 字节（CRC32 + ISIZE）。响应只有几百字节，用静态缓冲区即可。
static bool gunzipBody(const String& in, String& out) {
  const uint8_t* p = (const uint8_t*)in.c_str();
  size_t n = in.length();

  if (n < 18 || p[0] != 0x1F || p[1] != 0x8B) {
    out = in;                                 // 没压缩（或太短），原样返回
    return true;
  }

  size_t off = 10;
  uint8_t flg = p[3];
  if (flg & 0x04) {                           // FEXTRA
    if (off + 2 > n) return false;
    size_t xlen = (size_t)p[off] | ((size_t)p[off + 1] << 8);
    off += 2 + xlen;
  }
  if (flg & 0x08) { while (off < n && p[off]) off++; off++; }   // FNAME
  if (flg & 0x10) { while (off < n && p[off]) off++; off++; }   // FCOMMENT
  if (flg & 0x02) off += 2;                                     // FHCRC
  if (off + 8 >= n) return false;             // 头 + 尾都不完整

  size_t deflateLen = n - off - 8;
  static uint8_t buf[4096];
  size_t outLen = tinfl_decompress_mem_to_mem(buf, sizeof(buf) - 1,
                                              p + off, deflateLen,
                                              TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  if (outLen == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
    Serial.printf("[wx] gunzip failed (raw %u bytes, flg 0x%02x)\n", (unsigned)n, flg);
    return false;
  }
  buf[outLen] = 0;
  out = String((const char*)buf);
  return true;
}

// 发一次 GET，返回响应体；失败返回 false 并打日志（日志里只出现 path，绝不出现 key）
static bool wxGet(const char* path, String& body) {
  WiFiClientSecure client;
  client.setInsecure();          // 不校验证书：省 RAM/时间，家用表盘可接受
  HTTPClient http;
  http.setTimeout(8000);
  http.setConnectTimeout(6000);
  if (!http.begin(client, QWEATHER_HOST, 443, path, true)) {
    Serial.printf("[wx] begin failed: %s\n", path);
    return false;
  }
  http.addHeader("X-QW-Api-Key", QWEATHER_KEY);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[wx] http %d  %s\n", code, path);
    http.end();
    return false;
  }
  String raw = http.getString();
  http.end();

  if (!gunzipBody(raw, body)) return false;
  if (body.indexOf("\"code\":\"200\"") < 0) {
    Serial.printf("[wx] unexpected body: %.60s\n", body.c_str());
    return false;
  }
  return true;
}

static bool wxFetch(WeatherData& w) {
  char path[96];
  String body;

  // 1) 实时天气
  snprintf(path, sizeof(path), "/v7/weather/now?location=%s", QWEATHER_CITY);
  if (!wxGet(path, body)) return false;
  w.temp = jsonInt(body, "temp", 0);
  w.hum  = jsonInt(body, "humidity", 0);
  w.icon = jsonInt(body, "icon", 999);
  jsonStr(body, "text", w.text, sizeof(w.text));
  int code = jsonInt(body, "code", 0);
  if (code != 200) { Serial.printf("[wx] now code=%d\n", code); return false; }

  // 2) 今日高低温（3 天预报里的第一天）
  snprintf(path, sizeof(path), "/v7/weather/3d?location=%s", QWEATHER_CITY);
  if (!wxGet(path, body)) return false;
  w.tmax = jsonInt(body, "tempMax", w.temp);
  w.tmin = jsonInt(body, "tempMin", w.temp);

  w.valid = true;
  return true;
}

// 后台任务：串口这边只有在"拿到数据 / 失败"时各打一行
static void weatherTask(void*) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    WeatherData w = {};
    if (wxFetch(w)) {
      portENTER_CRITICAL(&g_wxMux);
      g_wx = w;
      portEXIT_CRITICAL(&g_wxMux);
      g_wxDirty = true;
      Serial.printf("[wx] %d°C  %s  湿度 %d%%  今日 %d~%d°C  icon %d  heap %u KB\n",
                    w.temp, w.text, w.hum, w.tmin, w.tmax, w.icon,
                    (unsigned)(ESP.getFreeHeap() / 1024));
      vTaskDelay(pdMS_TO_TICKS(WX_REFRESH_MS));
    } else {
      Serial.println("[wx] fetch failed, retry in 60s");
      vTaskDelay(pdMS_TO_TICKS(WX_RETRY_MS));
    }
  }
}

// ============================================================
// 天气图标（全部用基本图形拼，不依赖字体/图片）
// ============================================================
static void drawCloud(int x, int y, int w, int h, uint16_t c) {
  // x,y 为外框左上角
  tft.fillCircle(x + w * 28 / 100, y + h * 62 / 100, h * 26 / 100, c);
  tft.fillCircle(x + w * 55 / 100, y + h * 42 / 100, h * 34 / 100, c);
  tft.fillCircle(x + w * 78 / 100, y + h * 62 / 100, h * 24 / 100, c);
  tft.fillRect(x + w * 28 / 100, y + h * 62 / 100, w * 50 / 100, h * 24 / 100, c);
}

static void drawSun(int cx, int cy, int r, uint16_t c) {
  tft.fillCircle(cx, cy, r, c);
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4;
    int x1 = cx + (int)((r + 3) * cos(a));
    int y1 = cy + (int)((r + 3) * sin(a));
    int x2 = cx + (int)((r + 8) * cos(a));
    int y2 = cy + (int)((r + 8) * sin(a));
    tft.drawLine(x1, y1, x2, y2, c);
  }
}

// code 用和风天气图标编号；x,y 是 52x52 图标框的左上角
static void drawWxIcon(int code, int x, int y, int s, uint16_t c) {
  tft.fillRect(x, y, s, s, BG_COLOR);
  int cx = x + s / 2, cy = y + s / 2;

  if (code == 100 || code == 150) {                       // 晴
    drawSun(cx, cy, s / 5, c);
  } else if ((code >= 101 && code <= 103) || (code >= 151 && code <= 153)) {  // 多云
    drawSun(x + s * 30 / 100, y + s * 30 / 100, s / 8, c);
    drawCloud(x + s * 15 / 100, y + s * 38 / 100, s, s * 55 / 100, c);
  } else if (code == 104 || code == 154) {                 // 阴
    drawCloud(x, y + s * 10 / 100, s, s * 60 / 100, c);
    drawCloud(x + s * 8 / 100, y + s * 38 / 100, s * 85 / 100, s * 50 / 100, c);
  } else if (code >= 300 && code <= 399) {                 // 雨
    drawCloud(x, y, s, s * 55 / 100, c);
    for (int i = 0; i < 3; i++) {
      int lx = x + s * (25 + i * 22) / 100;
      tft.drawLine(lx, y + s * 62 / 100, lx - 3, y + s * 82 / 100, c);
    }
  } else if (code >= 400 && code <= 499) {                 // 雪
    drawCloud(x, y, s, s * 55 / 100, c);
    for (int i = 0; i < 3; i++) {
      tft.fillCircle(x + s * (27 + i * 22) / 100, y + s * 74 / 100, 3, c);
    }
  } else if (code >= 500 && code <= 515) {                 // 雾 / 霾
    drawCloud(x, y, s, s * 45 / 100, c);
    for (int i = 0; i < 3; i++) {
      int ly = y + s * (58 + i * 14) / 100;
      tft.drawLine(x + s * 15 / 100, ly, x + s * 85 / 100, ly, c);
    }
  } else if (code >= 200 && code <= 299) {                 // 风
    for (int i = 0; i < 3; i++) {
      int ly = y + s * (32 + i * 18) / 100;
      tft.drawLine(x + s * 12 / 100, ly, x + s * (72 - i * 8) / 100, ly, c);
      tft.drawCircle(x + s * (76 - i * 8) / 100, ly - s * 4 / 100, s * 6 / 100, c);
    }
  } else {                                                 // 其它：一朵云
    drawCloud(x, y + s * 15 / 100, s, s * 60 / 100, c);
  }
}

// 数字 + 手绘小圈当"度"
static void drawTemp(int value, int x, int cy, int font, int r, uint16_t color) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", value);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, BG_COLOR);
  tft.drawString(buf, x, cy, font);
  int w = tft.textWidth(buf, font);
  tft.drawCircle(x + w + r + 3, cy - font * 3 / 10, r, color);
}

static void drawWeather() {
  WeatherData w;
  portENTER_CRITICAL(&g_wxMux);
  w = g_wx;
  portEXIT_CRITICAL(&g_wxMux);

  tft.fillRect(0, WX_Y_TOP, tft.width(), tft.height() - WX_Y_TOP, BG_COLOR);
  tft.drawFastHLine(0, WX_Y_TOP, tft.width(), LINE_COLOR);

  if (!w.valid) {
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(FG_DIM, BG_COLOR);
    tft.drawString("weather: waiting...", 14, WX_Y_TOP + 36, 2);
    return;
  }

  drawWxIcon(w.icon, 14, WX_Y_TOP + 12, 52, FG_COLOR);

  drawTemp(w.temp, 80, WX_Y_TOP + 36, 6, 5, FG_COLOR);

  char buf[24];
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(FG_DIM, BG_COLOR);
  snprintf(buf, sizeof(buf), "%d°/%d°", w.tmax, w.tmin);
  tft.drawString(buf, 310, WX_Y_TOP + 20, 2);
  snprintf(buf, sizeof(buf), "RH %d%%", w.hum);
  tft.drawString(buf, 310, WX_Y_TOP + 46, 2);
}

// ============================================================
// 屏幕：初始化 + 静态框架
// ============================================================
// 开机用背光淡入代替以前的"闪三下"：先把画面画好，再让亮度 0→255 平滑升起，
// 比起闪烁更像正经设备。PWM 走 LEDC，GPIO7 是 TFT_BL（高电平点亮）。
static void backlightFadeIn() {
#ifdef TFT_BL
  for (int d = 0; d <= 255; d += 4) {
    ledcWrite(TFT_BL, TFT_BACKLIGHT_ON ? d : 255 - d);
    delay(5);                    // 64 档 x 5ms ≈ 320ms
  }
  ledcWrite(TFT_BL, TFT_BACKLIGHT_ON ? 255 : 0);
#endif
}

static void displayInit() {
#ifdef TFT_BL
  ledcAttach(TFT_BL, 5000, 8);   // 5kHz, 8bit
  ledcWrite(TFT_BL, 0);          // 初始化期间先全黑，避免上电花屏被人看到
#endif

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);
  Serial.printf("display: %d x %d\n", tft.width(), tft.height());

  tft.drawFastHLine(0, TOP_BAR_H, tft.width(), LINE_COLOR);

  Serial.println("backlight: fade in (GPIO7 PWM)");
  backlightFadeIn();
}

// 右上角 WiFi 图标：-2 = 还没画过；-1 = 未连接；1..4 = 信号等级
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
  if (bars == g_lastBars) return;
  g_lastBars = bars;

  const int x0 = 294, base = 30, w = 3, gap = 2;
  const int h[4] = {3, 5, 7, 9};
  tft.fillRect(288, 16, 26, 20, BG_COLOR);

  for (int i = 0; i < 4; i++) {
    uint16_t c = (bars > 0 && i < bars) ? FG_COLOR : 0x9CF3;
    tft.fillRect(x0 + i * (w + gap), base - h[i], w, h[i], c);
  }
  if (bars < 0) {
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

  static int lastSec = -1;
  if (t.tm_sec == lastSec) return;
  lastSec = t.tm_sec;

  char hms[16], date[32];
  strftime(hms, sizeof(hms), "%H:%M:%S", &t);
  strftime(date, sizeof(date), "%Y-%m-%d", &t);

  static char lastTop[20] = "";
  char top[20];
  strftime(top, sizeof(top), "%m/%d  %a", &t);
  if (strcmp(top, lastTop) != 0) {
    strcpy(lastTop, top);
    tft.fillRect(0, 0, 180, TOP_BAR_H - 1, BG_COLOR);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(FG_DIM, BG_COLOR);
    tft.drawString(top, 10, TOP_BAR_H / 2, 2);
  }

  // 秒和时分用同一个大字号，整行 HH:MM:SS 居中
  char hm[10];
  strftime(hm, sizeof(hm), "%H:%M:%S", &t);
  drawTimeBig(hm);

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
  Serial.println(" Stage 3 : clock + QWeather");
  Serial.println("========================================");

  displayInit();
  drawWeather();                 // 先画一次 "waiting"

  Serial.println("WiFi: starting (non-blocking)");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  wifiBeginConnect();

  // 天气任务放 core 0，loop（表盘）在 core 1，互不干扰。
  // ⚠️ 栈必须给足 16KB：TLS 握手要 10KB 以上，8192 会触发
  //    "Stack canary watchpoint triggered (weather)" 然后整机重启
  xTaskCreatePinnedToCore(weatherTask, "weather", 16384, nullptr, 1, nullptr, 0);
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

  // ---------- 3. 右上角 WiFi 图标按需刷新 ----------
  static uint32_t lastStatus = 0;
  if (g_statusDirty || millis() - lastStatus > STATUS_REFRESH_MS) {
    lastStatus = millis();
    drawWifiIcon();
  }

  // ---------- 4. 天气有新数据才重画（与秒针刷新解耦）----------
  if (g_wxDirty) {
    g_wxDirty = false;
    drawWeather();
  }

  // ---------- 5. 每秒刷一次表盘 ----------
  if (valid) {
    updateClock();
  } else {
    static uint32_t lastWarn = 0;
    if (WiFi.status() == WL_CONNECTED && millis() - lastWarn > 5000) {
      lastWarn = millis();
      Serial.println("[time] not synced yet");
    }
  }

  delay(10);
}
