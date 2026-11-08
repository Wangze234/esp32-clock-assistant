# 屏幕档案：MSP2807 2.8" SPI TFT（ILI9341 + XPT2046）

> 依据：`data/C3-4-4L/` 中的商品封面、结构图（MSP2807_SIZE.png）、原理图（MSP2807-2.8-SPI.pdf）、规格书（QD-TFT2803）、Arduino 例程。
> 核对日期：2026-11-06

---

## 1. 一句话结论

**2.8 英寸、320×240、ILI9341 驱动、4 线 SPI、带 XPT2046 电阻触摸和 SD 卡槽**的模块。带触摸版本型号 **MSP2807**（无触摸是 MSP2806）。PCB 50×86mm，四角有 3mm 安装孔，和原计划的外壳开孔完全对得上。

---

## 2. 基本参数

| 项目 | 参数 |
|---|---|
| 型号 | MSP2807（带触摸） |
| 显示尺寸 | 2.8 英寸 |
| 驱动 IC | **ILI9341** |
| 分辨率 | **320 × 240** |
| 颜色 | RGB 65K（16bit） |
| 接口 | **4 线 SPI**（CS / DC / SCK / MOSI + RST） |
| 有效显示区 | **43.2 × 57.6 mm** |
| 模块 PCB | **50.0 × 86.0 mm** |
| 安装孔 | 4 个，⌀3.0 mm |
| 触摸 | **XPT2046 电阻触摸**（SPI 接口，带 T_IRQ） |
| 背光 | 冷白 LED，板载三极管驱动，**高电平点亮，可 PWM 调光** |
| 存储扩展 | Micro SD 卡槽（SPI） |
| 供电 | VCC 3.3V ~ 5V |
| 逻辑电平 | 3.3V（TTL） |
| 工作温度 | -20 ~ 60°C |
| 重量 | 带触摸约 43g |

---

## 3. 接口定义（14 针 J2）

按原理图 J2 的顺序（丝印以实物为准）：

| 针号 | 丝印 | 功能 | 说明 |
|---|---|---|---|
| 1 | VCC | 电源正 | 3.3V 或 5V，见 §4 的 J1 跳线 |
| 2 | GND | 地 | |
| 3 | TFT_CS | 显示片选 | |
| 4 | TFT_RESET | 显示复位 | |
| 5 | TFT_RS / DC | 数据/命令 | |
| 6 | TFT_SDI / MOSI | SPI 数据输入 | |
| 7 | TFT_SCK | SPI 时钟 | |
| 8 | TFT_BL / LED | 背光控制 | 高电平亮 |
| 9 | TFT_SDO / MISO | SPI 数据输出 | 只写显示时可不接 |
| 10 | TP_CLK | 触摸 SPI 时钟 | 可与 TFT_SCK 共用 |
| 11 | TP_CS | 触摸片选 | 独立 |
| 12 | TP_DIN | 触摸 SPI 数据输入 | 可与 TFT_SDI 共用 |
| 13 | TP_OUT / T_DO | 触摸 SPI 数据输出 | 接 MISO |
| 14 | TP_IRQ | 触摸中断 | 低有效，可选 |

> ⚠️ 收到的实物如果是 **10 针**而不是 14 针，说明是**无触摸版 MSP2806**，触摸相关引脚不存在，其余显示功能不受影响。开箱先数针脚。

---

## 4. 板载电路（原理图要点）

### 4.1 供电与 J1 跳线（最重要）

板上有 **XC6206 3.3V LDO**。J1 是背面的一个焊盘跳线，用来选择供电方式：

| 外部 VCC 输入 | J1 状态 | 原因 |
|---|---|---|
| **5V** | **断开** | 5V 经 XC6206 降压到 3.3V |
| **3.3V** | **短接** | 绕过 LDO，VCC 直连 3.3V 轨 |

按上表，本项目若接 3.3V，J1 应当短接。资料包里那个 0 字节文件 `屏幕背面J1焊点需要短接.txt` 就是在提醒这件事。

> 🚨 **J1 短接后绝对不能接 5V**，否则 5V 会直接加到 3.3V 轨上，烧屏。
> 开箱先用万用表蜂鸣档量 VCC 与板上 3.3V 点：导通 = 已短接；不导通 = 需要自己补焊。

> **⚠️ 本机实测（2026-11-08），与上表不一致，务必看这条：**
> - 照片看 J1 焊盘像断开的；但 **VCC 接 5V 时屏幕只有背光、无画面**，**改接 3V3 后画面正常**。
> - 也就是说：**实测能用的是 3V3**，而上表在"J1 断开"时推荐 5V —— 照片判断不可靠，J1 真实状态未用万用表确认。
> - 结论：**保持 VCC 接 3V3，不要接 5V**。3V3 在 J1 短接/断开两种情况下都不会烧屏（断开时过 LDO 略有压降，实测不影响显示）。
> - 有万用表后量一次「VCC 引脚 ↔ 板上 3.3V 点」把结论补上。
> - 另注：黑屏期间曾判定"模块不良"，实际重新插拔杜邦线后恢复，**不是硬件损坏**（见 README §6 #18）。

### 4.2 背光电路

```
3.3V ── LEDA ── 背光LED ── LEDK ── R5(10Ω) ── Q1(SS050)集电极
                                                │
TFT_BL ── R6(1k) ── Q1 基极                 Q1 发射极 ── GND
```

结论：
- **TFT_BL 高电平 = 背光亮**，低电平熄灭
- 有独立三极管，GPIO 只驱动基极（约 3mA），**可以直接接 GPIO，也可以 PWM 调光**
- 不要用 GPIO 直接灌 LED 电流

### 4.3 触摸电路

- 控制器 **XPT2046**，四线电阻触摸
- 与显示共用 SPI 总线（CLK/MOSI 可并联），**T_CS 必须独立**
- T_IRQ 有 10k 上拉到 3.3V，按下时拉低
- 触摸和显示不能同时选中（靠各自的 CS 分时复用）

### 4.4 SD 卡

- 独立片选 SD_CS，SPI 与显示共用
- 本项目暂时不用，后续要存天气图标/图片时再启用

---

## 5. 与 ESP32-S3 的接线分配

屏幕信号全部接在开发板 **J1 排针**一侧，走线短。

| 屏幕引脚 | → ESP32-S3 | 说明 |
|---|---|---|
| VCC | 3V3 | 已验证可显示；不要接 5V（见 §4.1 实测注） |
| GND | GND | |
| TFT_CS | GPIO 10 | |
| TFT_RESET | GPIO 8 | |
| TFT_RS / DC | GPIO 9 | |
| TFT_SDI / MOSI | GPIO 11 | 与 TP_DIN 共用 |
| TFT_SCK | GPIO 12 | 与 TP_CLK 共用 |
| TFT_BL | GPIO 7 | 高电平亮 |
| TFT_SDO | 不接 | 显示不需要读 |
| TP_CLK | GPIO 12 | 与 TFT_SCK 并联 |
| TP_DIN | GPIO 11 | 与 TFT_SDI 并联 |
| TP_OUT | GPIO 13 | 作为 SPI MISO |
| TP_CS | GPIO 14 | 触摸片选 |
| TP_IRQ | GPIO 47 | 可选，不接也能轮询 |

**接线技巧**：TP_CLK 和 TFT_SCK 都插到面包板上同一个 GPIO 所在的孔列；TP_DIN 和 TFT_SDI 同理。这样只多占 3 个 GPIO（13/14/47）。

---

## 6. TFT_eSPI 配置要点

`User_Setup.h` 关键宏：

```cpp
#define ILI9341_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8
#define TFT_BL    7
#define TFT_BACKLIGHT_ON HIGH

#define TOUCH_CS 14          // 启用 XPT2046 触摸
#define SPI_FREQUENCY  40000000     // 单位 Hz！40000 = 40kHz，会慢到一屏 10 秒
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000
```

注意事项：
- **驱动必须选 ILI9341**，不是 ST7789
- 触摸与显示共用 MOSI/SCLK，TFT_eSPI 默认就支持
- 若花屏：SPI_FREQUENCY 降到 27000000
- ⚠️ `SPI_FREQUENCY` 等宏的单位是 **Hz**（库默认 27000000）。本机实测：误写成 40000 时屏幕能显示但每屏填充 ~10 秒，改对后 32 ms
- 若颜色反了：在 User_Setup 里加 `#define TFT_INVERSION_ON`
- 横屏用 `tft.setRotation(1)`，得到 320×240

---

## 7. 开箱检查清单

- [ ] 数针脚：**14 针**是带触摸 MSP2807；10 针是无触摸 MSP2806
- [ ] 看背面 J1 焊盘是否已短接（万用表量 VCC 与 3.3V 点）
- [ ] 确认排针已焊、无歪针
- [ ] 屏幕正面无划伤、无气泡
- [ ] 量 PCB：应为 50 × 86 mm，四角孔径 3 mm
- [ ] 拍照存 `hardware/photos/`

---

## 8. 资料包内容说明

| 目录 | 内容 | 是否有用 |
|---|---|---|
| `1-Demo/Demo_Arduino/` | LCDWIKI 库 + 10 个 Arduino 例程（显示、触摸、SD、滚动等） | ✅ 可参考 |
| `2-Specification/` | QD-TFT2803 规格书（14 页） | 亮度/时序参考 |
| `3-Structure_Diagram/` | 结构尺寸图 | ✅ 开壳用 |
| `4-Driver_IC_Data_Sheet/` | ILI9341 数据手册 | 查寄存器时用 |
| `5-Schematic/` | 原理图 + Altium 库 | ✅ 已提取 |
| `6-User_Manual/` | Arduino/Keil/取模工具的使用说明 | 选看 |
| `7-Character&Picture_Molding_Tool/` | 图片/汉字取模工具（Windows） | 后续做字库用 |

> 例程是给 **Arduino UNO** 写的（引脚用 A0–A5），不能直接烧到 ESP32-S3。我们只用它参考初始化流程和引脚定义，实际驱动用 TFT_eSPI。

---

## 9. 与外壳的匹配

| 项目 | 数值 | 对应外壳开孔 |
|---|---|---|
| 可视区 | 57.6 × 43.2 mm | 开 **58 × 44 mm** |
| PCB | 50 × 86 mm | 100×68×50 盒子可放下（86 沿 100mm 方向） |
| 安装孔 | 4 × ⌀3.0 mm | M3 螺丝或自攻螺丝 |
| 排线方向 | 底部 14 针 | 外壳底部留出线缆空间 |
