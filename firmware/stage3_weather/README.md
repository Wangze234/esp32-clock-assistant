# 阶段 3：时钟 + 天气

> 目标：在阶段 2 的时钟上加天气（当前温度 / 天气图标 / 今日高低 / 湿度），并开始做日历。
> 当前状态：**天气部分已完成并上屏（2026-11-08）；日历部分未做。**
> 屏幕配置复用 `../stage1_display/User_Setup.h`，必须先把它覆盖到 TFT_eSPI 库里。

---

## 0. 这一版做了什么

1. 时钟字体换成**手绘七段数码管**：`fillRoundRect` 画每一笔，只要一点点圆角、整体仍然是方的；时分秒**同一字号**。
2. 接和风天气：实时天气 + 今日高低温，30 分钟刷新一次（失败 1 分钟后重试）。
3. 天气请求跑在**独立 FreeRTOS 任务**（core 0），表盘（core 1）永不被 HTTPS 握手卡住。

```
┌────────────────────────────────────────┐
│ 11/08  Sun                    ▁▃▅█     │  ← 顶栏 40px：左日期，右 WiFi 图标
├────────────────────────────────────────┤
│   1 9 : 0 4 : 1 1                      │  ← HH:MM:SS 全同字号（40×84 每字）
│                                        │
├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤  ← WX_Y_TOP = 168
│  ☁   7°          -2°/7°                │  ← 图标 / 当前温度 / 今日低~高
│                  RH 29%                │  ← 湿度
└────────────────────────────────────────┘
```

| 元素 | 说明 |
|---|---|
| 大号时间 | `SEG_W=40 / SEG_H=84 / SEG_T=11 / SEG_GAP=6 / SEG_COLON=12 / SEG_R=4`；总宽 294px，320 屏放得下 |
| 居中方式 | **光学居中**：`segInkLeft()` 把 "1" 字框里空着的左半边算进左边距，否则 19:xx 看着偏左 |
| 刷新方式 | **按格 diff**：只重画变化的那一格（一般是个位秒），不整条清屏；整条重画只在串宽/偏移变化时发生 |
| 开机 | 背光 PWM 淡入约 320ms（`ledcAttach(TFT_BL, 5000, 8)`），不再闪三下 |
| 想改风格 | 只要改这几个常量：`SEG_R` 设 0 就是纯直角笔画；`SEG_W/SEG_H` 改大小 |
| 天气图标 | 用圆/线/矩形现画，按和风图标编号分：晴 / 多云 / 阴 / 雨 / 雪 / 雾霾 / 风 |
| 天气区 | y = 168..240；阶段 4 的室内温湿度也放在这一条（右边两块目前是户外高低温和湿度） |

> TFT_eSPI 内置字体只有 ASCII，中文（多云/阴/小雨）只打串口，屏幕上用图标 + 数字表达。

---

## 1. 配置（不写进主文件）

`secrets.h` 由 `../common/secrets.example.h` 复制而来，已被 `.gitignore` 排除：

```cpp
#define WIFI_SSID       "……"
#define WIFI_PASSWORD   "……"
#define QWEATHER_KEY    "……"                          // 控制台里的 API KEY，32 位
#define QWEATHER_CITY   "101010100"                    // 北京
#define QWEATHER_HOST   "xxxxxxxx.re.qweatherapi.com"  // 专属 API Host，不是 devapi
```

> ⚠️ 2024 之后的和风天气**必须用控制台首页给你的专属 API Host**，
> 用 `devapi.qweather.com` 会返回 401。请求头用 `X-QW-Api-Key`。
> 这三项都不能出现在代码里，也不能打印到串口。

---

## 2. 两个必须知道的坑（改代码前先看）

### 2.1 响应永远是 gzip

实测：即使请求头写 `Accept-Encoding: identity`，和风天气照样返回 gzip（第一、二字节 `1f 8b`）。
ESP32 的 `HTTPClient::getString()` **不会自动解压**，直接按 JSON 找 `"code"` 会静默失败。

本目录的做法：

```cpp
#include <miniz.h>   // ROM 里的 tinfl
// 跳过 10 字节 gzip 头（+FEXTRA/FNAME/FCOMMENT/FHCRC）和尾部 8 字节，按 raw deflate 解压
tinfl_decompress_mem_to_mem(buf, sizeof(buf), p + off, deflateLen,
                            TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
```

### 2.2 HTTPS 任务的栈必须 ≥ 16KB

8192 时每次请求都会 `Stack canary watchpoint triggered (weather)`，然后整机反复重启（现象是串口每 ~9 秒重新打印一次启动日志）。现在是 `xTaskCreatePinnedToCore(..., 16384, ...)`。

---

## 3. 烧录与预期输出

Arduino IDE 打开 `stage3_weather.ino`，参数与阶段 0 相同，端口 `/dev/cu.wchusbserial*`，115200。

```
========================================
 Stage 3 : clock + QWeather
========================================
display: 320 x 240
backlight: fade in (GPIO7 PWM)
WiFi: starting (non-blocking)
WiFi: connecting to "……"
NTP: OK (took 3011 ms since boot)
WiFi: OK  IP=192.168.1.131  RSSI=-45 dBm
NTP: re-syncing after reconnect ...
2026-11-08 19:04:10  周日  RSSI -46 dBm  heap 240 KB
[wx] 7°C  多云  湿度 29%  今日 -2~7°C  icon 151  heap 240 KB
```

---

## 4. 验收

- [x] 屏幕显示 HH:MM:SS，秒数与时分一样大（2026-11-08）
- [x] 整体视觉居中（首位是 1 时也不再偏左）
- [x] 秒针跳动时没有整屏闪烁（按格 diff）
- [x] 开机背光淡入，不再闪三下
- [x] 断网时表盘照常跳（天气任务只在联网时工作）
- [x] 天气成功上屏：图标 / 当前温度 / 今日高低 / 湿度
- [x] 天气失败不影响时钟，60 秒后自动重试
- [x] 连续运行不再重启（栈 16KB），heap 稳定在 ~240KB
- [ ] 日历（待做）

---

## 5. 故障排查

| 现象 | 原因 | 解决 |
|---|---|---|
| 每 ~9 秒重新打印启动日志 | 天气任务栈溢出（`Stack canary watchpoint triggered (weather)`） | 任务栈 ≥ 16384 |
| `[wx] gunzip failed` | 跳 gzip 头的偏移算错，或响应被截断 | 看日志里的 raw 字节数；确认 `off` 跳过了 FEXTRA/FNAME/FCOMMENT/FHCRC |
| `[wx] http 401` | API Host 不是专属域名 / key 错 | 用控制台首页的 `xxxx.re.qweatherapi.com`，不要用 `devapi` |
| `[wx] http 403` / `unexpected body` | 请求头不对 | 用 `X-QW-Api-Key`，不要拼 `?key=` 参数 |
| 屏幕底部一直 `weather: waiting...` | 还没联网，或第一次请求失败 | 看串口有没有 `[wx] fetch failed`，60 秒后会自动重试 |
| 时间显示 `11:02` 而本地是 `19:02` | WiFi 刚连上、`configTime` 还没生效的那几秒 | 正常现象，NTP 同步完成后自动跳到本地时间 |

---

## 6. 下一步

- 日历：按 README §4 阶段 3 的验收要求补（公历日期已在顶栏；要做的是农历/节气或月视图，需要中文字库 → 也可以顺延到阶段 5 用 LVGL 字库一起做）。
- 阶段 4：AHT20 温湿度 + 备电 RTC，室内数据放到天气区右侧。
