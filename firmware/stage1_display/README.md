# 阶段 1：点亮屏幕（MSP2807）

> 目标：让屏幕显示纯色、色条和文字，并验证触摸能读到坐标。
> 预计耗时：30 分钟（含接线）。
> 前置：阶段 0 已完成（Arduino IDE + ESP32 支持包 + Blink 编译通过）。
> 当前状态：**2026-11-08 全部验收通过** —— 显示正常（整屏填充 32ms @ 40MHz）、8 色条正确、触摸 Z 压力检测可用、触摸坐标已标定（屏上打点与指甲位置吻合）。

---

## 0. 先安装 TFT_eSPI 库

目前这台电脑还没装 TFT_eSPI，先补上：

1. Arduino IDE 左侧点 **Library Manager**（书本图标）
2. 搜索 `TFT_eSPI`
3. 选作者为 **Bodmer** 的那个，点 **Install**
4. 装完后库路径应该是：

```
/Users/wangzeyi/Documents/Arduino/libraries/TFT_eSPI
```

> 如果搜不到，检查网络或 IDE 的库索引是否加载完成（重启 IDE 再试）。

---

## 1. 接线（断电操作）

按下面接，**接完检查一遍再上电**。屏幕 14 针。

> ⚠️ **VCC 接 `3V3`**（2026-11-08 实测：接 5V 只有背光、无画面；接 3V3 画面正常）。
> J1 跳线状态未用万用表确认，3V3 在两种状态下都不会烧屏，**别改回 5V**。详见 README §3.5。

| 屏幕引脚 | ESP32-S3 | 颜色建议 |
|---|---|---|
| VCC | **3V3** | 红 |
| GND | GND | 黑 |
| TFT_CS | GPIO 10 | 橙 |
| TFT_RESET | GPIO 8 | 黄 |
| TFT_RS / DC | GPIO 9 | 绿 |
| TFT_SDI / MOSI | GPIO 11 | 蓝 |
| TFT_SCK | GPIO 12 | 紫 |
| TFT_BL | GPIO 7 | 灰 |
| TP_DIN | GPIO 11（与 MOSI 同列） | 蓝 |
| TP_CLK | GPIO 12（与 SCK 同列） | 紫 |
| TP_OUT | GPIO 13 | 白 |
| TP_CS | GPIO 14 | 棕 |
| TP_IRQ | GPIO 47（可选） | — |
| TFT_SDO | 不接 | — |

接线要点：

- **VCC 接 3V3**（实测唯一能正常显示的接法，见 README §3.5）。不要接 5V。
- TP_DIN 和 TFT_SDI 插在面包板同一列；TP_CLK 和 TFT_SCK 同理。
- 屏幕排针在底部，面包板要跨过中间槽，别让排针短路。
- 上电前再确认一次 VCC/GND 没接反。

---

## 2. 替换 TFT_eSPI 的 User_Setup.h

这一步是把屏幕的引脚和驱动告诉 TFT_eSPI 库。

### 2.1 找到库目录

本机确认路径：

```
/Users/wangzeyi/Documents/Arduino/libraries/TFT_eSPI/
```

如果换了电脑，打开 Arduino IDE → **Settings**，看 **Sketchbook location** 的值，库在它下面的 `libraries/TFT_eSPI/`。

### 2.2 备份并覆盖

打开终端执行（路径按实际情况调整）：

```bash
cd ~/Documents/Arduino/libraries/TFT_eSPI
cp User_Setup.h User_Setup.h.bak
cp /Users/wangzeyi/Documents/esp32-clock-assistant/firmware/stage1_display/User_Setup.h ./User_Setup.h
```

确认一下内容：

```bash
grep -E "ILI9341_DRIVER|TFT_CS|TOUCH_CS" User_Setup.h
```

应该看到：

```
#define ILI9341_DRIVER
#define TFT_CS   10
#define TOUCH_CS 14
```

### 2.3 重启 Arduino IDE

改完库文件后必须重启 IDE，让它重新扫描库。保险起见再执行一次 **Sketch → Verify**。

> ⚠️ 这个改动对这台电脑上所有用 TFT_eSPI 的项目生效。想恢复原配置：
> `cp User_Setup.h.bak User_Setup.h`

---

## 3. 打开并烧录

1. Arduino IDE 打开 `stage1_display.ino`
2. 确认开发板：**ESP32S3 Dev Module**
3. 确认端口：`/dev/cu.wchusbserial*`（不是 `debug-console`）
4. 点 **Upload**
5. 打开 **Serial Monitor**，波特率 **115200**

---

## 4. 预期现象

### 屏幕

依次出现：

1. 红 → 绿 → 蓝 → 白 → 黑，每次约 0.7 秒，中间有大字
2. 8 条竖色条（红橙黄绿青蓝紫白）
3. 停在信息页：

```
      MSP2807 / ILI9341
    320 x 240  4-wire SPI
         Stage 1 OK
       touch the screen
            ●
```

### 串口

```
========================================
 Stage 1 : MSP2807 / ILI9341 self test
========================================
backlight: ON (GPIO7)
resolution: 320 x 240
fill: RED
fill: GREEN
fill: BLUE
fill: WHITE
fill: BLACK
color bars drawn
display test done, touch test running...
press the screen to see raw coordinates
```

用手指/指甲按屏幕，会打印（`z` 是压力值，只有 `z > 400` 才算真的按下）：

```
TOUCH DOWN: raw x=2283 y=3631 z=603  -> screen 3,109
TOUCH UP:   last  x=3078  y=3543  z=880
[touch] 5s pressed range: x 1081..1424 (span 343)  y 2536..2589 (span 53)
```

**屏幕上会在按下的位置画一个红点** —— 红点必须落在你指甲按的地方。
如果红点和手指位置 180° 颠倒（按左上、点跑到右下），说明触摸轴映射反了，
改 `rawToScreenX()` / `rawToScreenY()` 里 `map()` 的起止值（见下面 §5 的标定结论）。

---

## 5. 验收标准

- [x] 五种纯色显示正确，无死区、无条纹
- [x] 8 色条颜色正确（红是红、蓝是蓝）
- [x] 信息页文字清晰，方向正确（横屏）
- [x] 串口打印 `resolution: 320 x 240`，整屏填充 32 ms（40MHz SPI）
- [x] 触摸按下时串口有 `TOUCH DOWN ... -> screen x,y` 输出
- [x] **屏上红点与指甲位置吻合**（触摸坐标标定正确）
- [x] 屏幕四角都能正常显示（无偏移）

### 本机触摸标定结论（2026-11-08 实测，别改）

| 项目 | 结论 |
|---|---|
| 按下判断 | `getTouchRawZ() > 400`（`getTouchRaw()` 的返回值恒为 true，不能用） |
| raw 有效范围 | 640 ～ 3650 |
| X 轴 | **用 raw y**，且 **raw y 越大越靠左** |
| Y 轴 | **用 raw x**，且 **raw x 越大越靠上** |
| 四角 raw 值 | 左上(648,633)　右上(613,3659)　左下(3248,706)　右下(3538,3639) |
| 映射代码 | `screenX = map(rawY, 640,3650, 320,0)`、`screenY = map(rawX, 640,3650, 240,0)` |

> 也就是说本屏触摸是 **X/Y 交换 + 双向反相**。`setRotation(1)` 不会自动处理这个，
> 后续阶段（按钮点击）直接用 `rawToScreenX/Y()` 这两个函数，不要再自己 map 一遍。

---

## 6. 故障排查

| 现象 | 原因 | 解决 |
|---|---|---|
| 全黑，但串口有输出 | **先怀疑杜邦线/面包板接触不良**（2026-11-08 实际就是这个问题，白排很久） | ① 全部拔下重新插、换个面包板孔位 ② 换一根线 ③ 量 GPIO7 是否高电平、屏幕 VCC 是否有 3.3V ④ 最后才怀疑模块 |
| 全白 | 驱动不对 / 复位没接 / CS 错 | 确认 `ILI9341_DRIVER`；检查 GPIO8/10 |
| 花屏、雪花 | SPI 太快 / 线太长 | `SPI_FREQUENCY` 从 40000000 降到 27000000 或 20000000 |
| 能显示但**慢得离谱**（一屏好几秒） | `SPI_FREQUENCY` 单位写错（40kHz 而非 40MHz） | 必须是 **Hz**：`40000000`；心跳行会打印每屏填充耗时，正常 ~32ms |
| 红蓝互换 | 颜色顺序 BGR | 打开 `#define TFT_RGB_ORDER TFT_BGR` |
| 黑白颠倒 | 需要反相 | 打开 `#define TFT_INVERSION_ON` |
| 图像上下/左右反 | 旋转方向不对 | 改 `tft.setRotation(0~3)` 试 |
| 编译报 `TOUCH_CS` 相关错误 | TFT_eSPI 版本太旧 | 库管理器升级 TFT_eSPI |
| 编译报引脚重复定义 | User_Setup.h 没替换成功 | 重新执行 §2.2，重启 IDE |
| 上传失败 | 没进下载模式 | 按住 BOOT → 点 RST → 松开 BOOT → 再上传 |
| 触摸没反应 | 用指尖肉按压力不够（电阻屏） | 换**指甲或笔帽**用力压，先看 `z` 是否 >400 |
| 触摸没反应（z 一直很小） | TP_CS/TP_OUT 接错 / 共用线没并 | 检查 GPIO14、GPIO13，TP_CLK/DIN 是否并联 |
| 触摸 raw 值一直在 8191/0 间乱跳 | MISO 悬空 | 检查 TP_OUT 是否接 GPIO13 |
| **红点和手指位置 180° 颠倒** | 触摸轴方向反了 | 对调 `rawToScreenX/Y()` 里 `map()` 的起止参数（本机正确值见 §5 标定表） |
| 红点位置整体偏移/够不到边 | raw 有效范围不对 | 依次点四个角，看 `raw x/y` 的极值，改 `RAW_MIN` / `RAW_MAX` |

> 改 `User_Setup.h` 后每次都要重启 IDE 并重新编译。

---

## 7. 通过之后

**阶段 1 已于 2026-11-08 全部验收通过**（显示 + 触摸标定）。

阶段 2 已在本目录旁边的 `firmware/stage2_clock/` 完成，复用同一套 `User_Setup.h`。

> 触摸相关的两个函数（`rawToScreenX()` / `rawToScreenY()`）和标定常量
> `RAW_MIN` / `RAW_MAX` 在阶段 3 之后做按钮点击时要**原样复制**过去，
> 不要重新推导 —— 本机这块屏的方向是"交换 + 双向反相"，凭直觉写必然 180° 颠倒。
