# 阶段 3：时钟 + 天气

> 目标：在阶段 2 的时钟上加天气（当前温度 / 天气图标 / 今日高低 / 湿度），并开始做日历。
> 当前状态：**天气部分已完成并上屏（2026-11-08），含自动定位、未来 3 天、NVS 缓存、断网红色"离线"标记；日历部分未做。**
> 屏幕配置复用 `../stage1_display/User_Setup.h`，必须先把它覆盖到 TFT_eSPI 库里。

---

## 0. 这一版做了什么

1. 时钟字体换成**手绘七段数码管**：`fillRoundRect` 画每一笔，只要一点点圆角、整体仍然是方的；时分秒**同一字号**。
2. 接和风天气：实时天气 + 未来 3 天，30 分钟刷新一次（失败 1 分钟后重试）。
3. 天气请求跑在**独立 FreeRTOS 任务**（core 0），表盘（core 1）永不被 HTTPS 握手卡住。
4. 自动定位：用公网出口 IP 查经纬度，结果存 NVS，只查一次。
5. 天气数据也存 NVS：开机先显示上次数据；断网时顶栏标红色"离线"。

```
┌────────────────────────────────────────┐
│ 11/08 周日          湿度 29%    ▁▃▅█   │  ← 顶栏 40px：日期 / 星期 / 湿度 / WiFi 图标
├────────────────────────────────────────┤
│   1 9 : 0 4 : 1 1                      │  ← HH:MM:SS 全同字号（40×84 每字）
│                                        │
├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤  ← WX_Y_TOP = 168
│  ☁   7°       [预留]     今天   -2/7   │  ← 图标 / 当前温度 / 中间留给后续图标
│                          明天   -3/11  │
│                          后天   -1/15  │
└────────────────────────────────────────┘
```

> 三天标签左边缘固定为 `WX_DAY_LABEL_X = 210`（`stage3_weather.ino` 顶部常量）。
> 从温度右侧到 x=210 之间（约 150..210）是**预留给后续图标**的位置，
> 阶段 4 的室内温湿度可以放这里，不用再挪三天预报。

| 元素 | 说明 |
|---|---|
| 大号时间 | `SEG_W=40 / SEG_H=84 / SEG_T=11 / SEG_GAP=6 / SEG_COLON=12 / SEG_R=4`；总宽 294px，320 屏放得下 |
| 居中方式 | **光学居中**：`segInkLeft()` 把 "1" 字框里空着的左半边算进左边距，否则 19:xx 看着偏左 |
| 刷新方式 | **按格 diff**：只重画变化的那一格（一般是个位秒），不整条清屏；整条重画只在串宽/偏移变化时发生 |
| 开机 | 背光 PWM 淡入约 320ms（`ledcAttach(TFT_BL, 5000, 8)`），不再闪三下 |
| 想改风格 | 只要改这几个常量：`SEG_R` 设 0 就是纯直角笔画；`SEG_W/SEG_H` 改大小 |
| 天气图标 | 用圆/线/矩形现画，按和风图标编号分：晴 / 多云 / 阴 / 雨 / 雪 / 雾霾 / 风 |
| 天气区 | y = 168..240：左侧图标 + 当前温度；右侧三行 `今天/明天/后天`（左边缘 `WX_DAY_LABEL_X = 210`），值是右对齐到 310 的 `低/高`。中间 150..210 预留给后续图标（阶段 4 室内温湿度等） |
| 三天小字 | **14px 圆角中文字库**（`wx_font.h`），由 `tools/gen_wx_font.py` 用 macOS ヒラギノ丸ゴ渲染，含 `今天明后 0-9 / - 湿度%离线 周日一二三四五六` 共 29 个字形，编译后约占 6KB（每个字形 211 字节 += 掩码；头文件源码约 33KB）。**1-bit 硬边掩码**，只画笔画像素；不做灰度抗锯齿，否则小字号灰边在屏上像阴影 |
| 顶栏日期 | 左边 `MM/DD`（内置字体 2），紧跟 **14px 中文小字星期** `周日`…`周六`；`tm_wday` 直接查 `WEEKDAY_IDX[7][2]` |
| 湿度 / 离线 | 顶栏右侧 `湿度 nn%`（14px 中文小字）；**断网时变成红色 `离线`**，表示下面显示的是 NVS 里的缓存 |
| 自动定位 | `ipinfo.io/json` 取 `"loc":"纬度,经度"` → 转成 `"经度,纬度"` 给和风；成功结果存 NVS `loc`，以后开机不再请求 |
| 缓存 | 每次成功请求把整个 `WeatherData`（含 3 天）存 NVS `wx`；开机先读出来画屏，再等联网刷新 |

> TFT_eSPI 内置字体只有 ASCII，中文（多云/阴/小雨）只打串口，屏幕上用图标 + 数字表达；
> 星期和顶栏的"湿度/离线"则走自建 14px 中文字库。
> 小字里**不能写 `'°'`**：字体 2 只有 ASCII 32..127，`'°'`(0xB0) 会越界成乱码。小字统一用 `-2/7`，大号温度用 `drawTemp()` 手绘圆圈。

---

## 1. 配置（不写进主文件）

`secrets.h` 由 `../common/secrets.example.h` 复制而来，已被 `.gitignore` 排除：

```cpp
#define WIFI_SSID       "……"
#define WIFI_PASSWORD   "……"
#define QWEATHER_KEY    "……"                          // 控制台里的 API KEY，32 位
#define QWEATHER_CITY   "101010100"                    // 自动定位失败时的兜底（北京）
#define QWEATHER_HOST   "xxxxxxxx.re.qweatherapi.com"  // 专属 API Host，不是 devapi
// 可选：不想自动定位就加这一行，改成 0
// #define QWEATHER_AUTO_LOCATION 0
```

> ⚠️ 2024 之后的和风天气**必须用控制台首页给你的专属 API Host**，
> 用 `devapi.qweather.com` 会返回 401。请求头用 `X-QW-Api-Key`。
> 这些都不能出现在代码里，也不能打印到串口。
>
> 自动定位用 `ipinfo.io`（HTTPS、免费、无需 key）。和风自己的 GeoAPI
> `/geo/v2/city/lookup` 在本账号是 **403**（套餐没开），所以不走它。
> 定位结果和天气缓存都在 NVS 命名空间 `clock` 里（键 `loc` / `wx`），
> 想重新定位：擦除 NVS 或改一次 `NVS_LOC` 的键名。

---

## 2. 几个必须知道的坑（改代码前先看）

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

### 2.3 ipinfo.io 的 JSON 是带空格的

和风天气返回紧凑 JSON（`"text":"多云"`），ipinfo.io 返回**美化 JSON**：

```json
"loc": "39.9110,116.3950"
```

冒号后有一个空格。用固定串 `"\"loc\":\""` 去找会永远失败（HTTP 却是 200）。
`jsonStr()` 统一跳过冒号前后的空白/换行，两个服务共用。

### 2.4 字体 2 里没有 `'°'`

TFT_eSPI 内置字体 2/4/6 只有 **ASCII 32..127** 这 96 个字形。`'°'` 是 0xB0，
会越界取到乱码。所以小字用 `-2/7` 这种格式，大号温度用 `drawTemp()` 手绘圆圈。

### 2.5 顶栏 `湿度` 与 `离线` 切换要清屏

`湿度 100%` 最宽（约 x=210..284），`离线` 只有 29px，只画新字会留下旧字左半截。
切换前固定 `fillRect(206, 6, 80, 28, BG_COLOR)` 清掉整块文字区。

### 2.6 中文小字是自己生成的

TFT_eSPI 内置字体没有中文，LVGL 字库又太重。三天预报 + 顶栏只需要
`今天明后 0-9 / - 湿度%离线 周日一二三四五六` 29 个字形，所以用 `tools/gen_wx_font.py`
（Pillow + macOS ヒラギノ丸ゴ，14px）生成 `wx_font.h`：

```bash
python3 tools/gen_wx_font.py > wx_font.h   # 改了字符或字号才需要重跑
```

每个字形是 14×15 的 **1-bit 掩码** + 前进宽度，固件里 `wxDrawGlyph()`
只对掩码为 1 的像素调 `drawPixel()`，背景由 `drawWeather()` 统一清屏。
刻意**不做灰度抗锯齿**：14px 下黑笔画周围的半透明灰边在屏上看起来像
一圈阴影/发虚，硬边反而更清楚。字形的圆角来自字体本身，不受影响。
要加字（比如"周"或 `'°'`）就改脚本里的 `CHARS`，同时在
`wxGlyphIndex()` 里加索引。

### 2.7 SNTP 和天气请求会抢 lwIP 锁

实测偶发整机重启，backtrace 是
`sntp_dns_found → dns_gethostbyname → udp_new_ip_type`，断言
`Required to lock TCPIP core functionality!`。
原因：WiFi 刚连上时 `configTime()` 让 SNTP 去解析 `ntp.aliyun.com`，
天气任务同时又在解析和风域名，两个线程同时进 lwIP。

现在 `wifiService()` 记录 `g_ntpSyncAt`，天气任务：
1. 首次启动先等 NTP 对时完成（最多 15 秒），再多等 2 秒；
2. 每次 `configTime()` 之后的 8 秒内不发任何请求。

代价是开机后天气晚几秒出现，换来不再偶发重启。

---

## 3. 烧录与预期输出

Arduino IDE 打开 `stage3_weather.ino`，参数与阶段 0 相同，端口 `/dev/cu.wchusbserial*`，115200。

第一次开机（NVS 里还没有缓存 / 定位）：

```
========================================
 Stage 3 : clock + QWeather
========================================
display: 320 x 240
backlight: fade in (GPIO7 PWM)
WiFi: starting (non-blocking)
WiFi: connecting to "……"
NTP: requesting time ...
WiFi: OK  IP=192.168.1.131  RSSI=-43 dBm
[geo] auto location -> 116.40,39.91
[wx] location = 116.40,39.91
[wx] 6°C 多云 湿度 28% | 今 -2/7 明 -3/11 后 -1/15 | loc 116.40,39.91 | heap 239 KB
NTP: OK (took 4384 ms since boot)
2026-11-08 19:38:36  周日  RSSI -45 dBm  heap 239 KB
```

之后的每次开机（定位和天气都从 NVS 读，屏幕立刻有数据）：

```
[wx] cached data loaded (stamp ...)
...
[geo] cached location 116.40,39.91
[wx] location = 116.40,39.91
[wx] 6°C 多云 湿度 28% | 今 -2/7 明 -3/11 后 -1/15 | loc 116.40,39.91 | heap 239 KB
```

> 断网开机时不会出现 `[wx] location` 和天气行，屏幕底部显示缓存数据，
> 顶栏湿度变成红色"离线"。

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
- [x] 自动定位：无配置城市时也能取到 `116.40,39.91`，与北京数据一致（2026-11-08）
- [x] 定位结果写 NVS：第二次开机打印 `[geo] cached location ...`，不再请求 ipinfo
- [x] 未来 3 天上屏（`今天/明天/后天`），实测 `-2/7 -3/11 -1/15`
- [x] 三天小字改为 14px 圆角中文字库（比原字体 2 小一号），1-bit 硬边、无灰边阴影（2026-11-08）
- [x] 顶栏星期改中文（`11/08 周日`），不再用内置字体的 `Sun/Mon`（2026-11-08）
- [x] 天气缓存写 NVS：开机先读缓存（串口 `[wx] cached data loaded`），再联网刷新
- [x] 规避 SNTP 与天气请求并发 DNS 的 lwIP 断言崩溃（首请求延后到对时完成，2026-11-08）
- [ ] 断网开机：屏幕显示缓存 + 顶栏红色"离线"（代码已实现，需拔网实测一次）
- [ ] 日历（待做）

---

## 5. 故障排查

| 现象 | 原因 | 解决 |
|---|---|---|
| 每 ~9 秒重新打印启动日志 | 天气任务栈溢出（`Stack canary watchpoint triggered (weather)`） | 任务栈 ≥ 16384 |
| `[wx] gunzip failed` | 跳 gzip 头的偏移算错，或响应被截断 | 看日志里的 raw 字节数；确认 `off` 跳过了 FEXTRA/FNAME/FCOMMENT/FHCRC |
| `[wx] http 401` | API Host 不是专属域名 / key 错 | 用控制台首页的 `xxxx.re.qweatherapi.com`，不要用 `devapi` |
| `[wx] http 403` / `unexpected body` | 请求头不对 | 用 `X-QW-Api-Key`，不要拼 `?key=` 参数 |
| `[geo] auto location failed` | ipinfo 不通，或 JSON 里找不到 `loc` | 先看有没有 `[geo] ipinfo http 4xx/5xx`；解析现在容忍空格，若 ipinfo 改格式就更新 `jsonStr()` 的键名 |
| 屏幕小字出现乱码/方块 | 小字里写了 `'°'` | 字体 2 只有 ASCII 32..127；改用 `-2/7`，大号"度"用手绘圆圈 |
| 顶栏残留旧字 | `湿度 28%` 切成 `离线` 后没清旧字 | 切换前 `fillRect(206, 6, 80, 28, BG_COLOR)`（见 §2.5） |
| `[wx] 3d: less than 3 days` | 3 天接口返回少于 3 段 | 检查 `"fxDate"` 出现次数；接口失败时整次刷新作废，保留旧数据 |
| 串口 `assert failed: udp_new_ip_type ... Required to lock TCPIP core functionality!` 后重启 | SNTP 解析 NTP 域名和天气任务同时进 lwIP | 保留 `g_ntpSyncAt` 保护：首次等 NTP 完成、`configTime()` 后 8 秒内不请求（见 §2.7） |
| 中文小字缺字/错位 | 改了 `CHARS` 但 `wxGlyphIndex()` 没同步 | 索引固定为 `今天明后`(0..3)、`0-9`(4..13)、`/`(14)、`-`(15)、`湿`(16)、`度`(17)、`%`(18)、`离`(19)、`线`(20)、`周`(21)、`日`(22)、`一`(23)…`六`(28)；中文用显式索引表（`DAY_CN` / `WEEKDAY_IDX`），不走 `wxGlyphIndex()`；重跑脚本后核对 |
| 屏幕底部一直 `weather: waiting...` | 还没联网，NVS 里也没有缓存 | 看串口有没有 `[wx] fetch failed`，60 秒后会自动重试；成功一次后以后开机就有缓存 |
| 时间显示 `11:02` 而本地是 `19:02` | WiFi 刚连上、`configTime` 还没生效的那几秒 | 正常现象，NTP 同步完成后自动跳到本地时间 |

---

## 6. 下一步

- **天气部分已收尾**：自动定位 / 3 天 / NVS 缓存 / "离线"标记都在本目录实现。
- 待实测项：拔掉路由器后开机，确认屏幕显示缓存数据、顶栏变"离线"（见 §4）。
- 日历：按 README §4 阶段 3 的验收要求补（公历日期已在顶栏；要做的是农历/节气或月视图，需要中文字库 → 也可以顺延到阶段 5 用 LVGL 字库一起做）。
- 阶段 4：AHT20 温湿度 + 备电 RTC，室内数据放到天气区右侧。
