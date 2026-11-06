# 阶段 1：点亮屏幕（MSP2807）

> 目标：让屏幕显示纯色、色条和文字，并验证触摸能读到坐标。
> 预计耗时：30 分钟（含接线）。
> 前置：阶段 0 已完成（Arduino IDE + ESP32 支持包 + Blink 编译通过）。
> 当前状态：已在 Arduino CLI + ESP32 core 3.3.12 下**编译通过**（348KB / 26%），尚未硬件验证。

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

按下面接，**接完检查一遍再上电**。屏幕 14 针，VCC 一定接 3V3。

| 屏幕引脚 | ESP32-S3 | 颜色建议 |
|---|---|---|
| VCC | 3V3 | 红 |
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

- **VCC 接 3V3，不是 5V**，并且屏幕背面 **J1 必须短接**（见 `hardware/screen-msp2807.md` §4.1）。
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

用手指按屏幕，会打印：

```
TOUCH raw: x=1832  y=2451
TOUCH released
```

（数值是原始 ADC，不用管大小，有变化就说明触摸通了。）

---

## 5. 验收标准

- [ ] 五种纯色显示正确，无死区、无条纹
- [ ] 8 色条颜色正确（红是红、蓝是蓝）
- [ ] 信息页文字清晰，方向正确（横屏）
- [ ] 串口打印 `resolution: 320 x 240`
- [ ] 触摸时串口有 raw 坐标输出
- [ ] 屏幕四角都能正常显示（无偏移）

---

## 6. 故障排查

| 现象 | 原因 | 解决 |
|---|---|---|
| 全黑，但串口有输出 | 背光没开 / J1 没短接 | 量 GPIO7 是否高电平；量屏幕 VCC 是否有 3.3V |
| 全白 | 驱动不对 / 复位没接 / CS 错 | 确认 `ILI9341_DRIVER`；检查 GPIO8/10 |
| 花屏、雪花 | SPI 太快 / 线太长 | `SPI_FREQUENCY` 从 40000 降到 27000 或 20000 |
| 红蓝互换 | 颜色顺序 BGR | 打开 `#define TFT_RGB_ORDER TFT_BGR` |
| 黑白颠倒 | 需要反相 | 打开 `#define TFT_INVERSION_ON` |
| 图像上下/左右反 | 旋转方向不对 | 改 `tft.setRotation(0~3)` 试 |
| 编译报 `TOUCH_CS` 相关错误 | TFT_eSPI 版本太旧 | 库管理器升级 TFT_eSPI |
| 编译报引脚重复定义 | User_Setup.h 没替换成功 | 重新执行 §2.2，重启 IDE |
| 上传失败 | 没进下载模式 | 按住 BOOT → 点 RST → 松开 BOOT → 再上传 |
| 触摸没反应 | TP_CS/TP_OUT 接错 / 共用线没并 | 检查 GPIO14、GPIO13，TP_CLK/DIN 是否并联 |
| 触摸一直有值 | MISO 悬空 | 检查 TP_OUT 是否接 GPIO13 |

> 改 `User_Setup.h` 后每次都要重启 IDE 并重新编译。

---

## 7. 通过之后

阶段 1 完成后进入阶段 2：WiFi + NTP 时钟。

届时会在本目录旁边新建 `firmware/stage2_clock/`，复用同一套 `User_Setup.h`。
