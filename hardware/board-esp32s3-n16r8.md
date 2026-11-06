# 开发板档案：YD-ESP32-S3 N16R8（欣薇商城）

> 依据：`data/ESP32-S3开发板N16R8焊接/` 中的原理图（SCH V1.4）、尺寸图（Metric/Inch）、型号对比图、封面图，以及随附的 Arduino 例程。
> 核对日期：2026-11-06

---

## 1. 一句话结论

这是一块 **YD-ESP32-S3 核心板**（不是乐鑫官方 DevKitC-1），核心模组为 **ESP32-S3-WROOM-1-N16R8**：16MB Flash + 8MB **Octal** PSRAM，双 USB-C（CH343 串口 + 原生 USB），板载 WS2812 RGB（GPIO48）。本项目所需引脚全部引出，**接线方案无需修改**。

---

## 2. 核心参数

| 项目 | 参数 |
|---|---|
| 模组型号 | ESP32-S3-WROOM-1-N16R8 |
| 芯片 | ESP32-S3，双核 Xtensa LX7，最高 240MHz |
| 内部 SRAM | 512 KB |
| 内部 ROM | 384 KB |
| 外部 Flash | 16 MB（Quad SPI） |
| 外部 PSRAM | 8 MB（**Octal SPI**） |
| 无线 | 2.4GHz Wi-Fi + Bluetooth 5.0（LE） |
| 工作温度 | -40 ~ 65°C（N16R8 版本） |
| 模组尺寸 | 18.0 × 25.5 × 3.1 mm |
| 开发板 PCB | 57.15 × 25.40 mm |
| 排针跨度 | 外沿 27.94 mm，针距 53.34 mm |
| 整体高度 | 63.39 mm（含天线区） |
| USB 转串口 | CH343P（需装 CH343 驱动） |
| 板载 RGB | WS2812B，接 **GPIO48** |
| 供电 | USB 5V，板载 AMS1117-3.3 LDO |

> ⚠️ **8MB PSRAM 是 Octal（八线）模式**，会占用 GPIO 33–37。这也是为什么这块板子的排针上**看不到 GPIO 33–37**——不是漏引，是被 PSRAM 占了，不能他用。

---

## 3. 板载资源

| 资源 | 说明 |
|---|---|
| USB-C ×2 | 一个是 **CH343 串口**（下载/串口监视器），一个是 **原生 USB-OTG**（GPIO19/20） |
| CH343P | USB 转 UART，对应芯片 U0TXD/U0RXD = **GPIO43/44** |
| BOOT 按键 | 接 **GPIO0**，低电平进入下载模式 |
| RST 按键 | 接 CHIP_PU，复位 |
| RGB LED | WS2812B，数据脚 **GPIO48** |
| 电源 LED | 红色，直连 3.3V |
| LDO | AMS1117-3.3，输入 5V，输出 3.3V |
| 天线 | 板载 PCB 天线；部分批次带 IPEX 座（随机发货） |

---

## 4. 完整引脚排布

板子为 2×22 排针（J1 左、J2 右），从上到下（USB 口朝下时为从下到上，以丝印为准）：

### J1（左侧排针）

| 针号 | 丝印 | 说明 |
|---|---|---|
| 1 | 3V3 | 3.3V 输出 |
| 2 | 3V3 | 3.3V 输出 |
| 3 | RST | CHIP_PU 复位 |
| 4 | GPIO4 | ✅ 可用 |
| 5 | GPIO5 | ✅ 可用 |
| 6 | GPIO6 | ✅ 可用 |
| 7 | GPIO7 | ✅ 可用 |
| 8 | GPIO15 | ✅ 可用 |
| 9 | GPIO16 | ✅ 可用 |
| 10 | GPIO17 | ✅ 可用 |
| 11 | GPIO18 | ✅ 可用 |
| 12 | GPIO8 | ✅ 可用 |
| 13 | GPIO3 | ⚠️ strapping（JTAG 选择） |
| 14 | GPIO46 | ⚠️ strapping |
| 15 | GPIO9 | ✅ 可用 |
| 16 | GPIO10 | ✅ 可用 |
| 17 | GPIO11 | ✅ 可用 |
| 18 | GPIO12 | ✅ 可用 |
| 19 | GPIO13 | ✅ 可用 |
| 20 | GPIO14 | ✅ 可用 |
| 21 | 5V | USB 5V 输出/输入 |
| 22 | GND | 地 |

### J2（右侧排针）

| 针号 | 丝印 | 说明 |
|---|---|---|
| 1 | GND | 地 |
| 2 | TX | U0TXD = **GPIO43**（CH343） |
| 3 | RX | U0RXD = **GPIO44**（CH343） |
| 4 | GPIO1 | ✅ 可用 |
| 5 | GPIO2 | ✅ 可用 |
| 6 | GPIO42 | ✅ 可用 |
| 7 | GPIO41 | ✅ 可用 |
| 8 | GPIO40 | ✅ 可用 |
| 9 | GPIO39 | ✅ 可用 |
| 10 | GPIO38 | ✅ 可用 |
| 11 | GPIO37 | ❌ 被 Octal PSRAM 占用 |
| 12 | GPIO36 | ❌ 被 Octal PSRAM 占用 |
| 13 | GPIO35 | ❌ 被 Octal PSRAM 占用 |
| 14 | GPIO0 | ⚠️ BOOT 按键 |
| 15 | GPIO45 | ⚠️ strapping |
| 16 | GPIO48 | ⚠️ 板载 RGB LED |
| 17 | GPIO47 | ✅ 可用 |
| 18 | GPIO21 | ✅ 可用 |
| 19 | GPIO20 | ❌ 原生 USB D+ |
| 20 | GPIO19 | ❌ 原生 USB D- |
| 21 | GND | 地 |
| 22 | GND | 地 |

> 注：原理图上 GPIO37/36/35 是画在排针上的（J2 11–13），但实际 N16R8 模组的 33–37 被 Octal PSRAM 占用，**不要使用**。以"能用"为准，保守处理。

---

## 5. GPIO 可用性分类

**✅ 本项目可直接使用（20 个）**
`4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42, 47`

**⚠️ 慎用**
- `0`：BOOT 按键，启动时拉低会进下载模式
- `3`、`45`、`46`：strapping 引脚，上电瞬间电平会影响启动模式
- `48`：板载 RGB LED，用作普通 IO 会闪灯
- `19`、`20`：原生 USB，插着 USB 线时不可用

**❌ 禁止使用**
- `33–37`：Octal PSRAM 占用（N16R8 特有）
- `43`、`44`：CH343 串口（TX/RX），占用后无法下载和看日志

---

## 6. Arduino IDE 具体配置

**开发板选择：`ESP32S3 Dev Module`**

| 选项 | 设置值 | 说明 |
|---|---|---|
| Flash Size | **16MB (128Mb)** | 对应 N16 |
| PSRAM | **OPI PSRAM** | 对应 R8（Octal），选错会内存不足 |
| Partition Scheme | **16M Flash (3MB APP/9.9MB FATFS)** | LVGL 字体较大，留足 APP 空间 |
| Upload Speed | 921600 | 不稳定就降到 460800 |
| USB CDC On Boot | **Disabled** | ⚠️ 用 CH343 的 COM 口时必须是 Disabled |
| CPU Frequency | 240MHz | |
| Flash Mode | QIO 80MHz | |
| JTAG Adapter | Disabled | |

> **为什么 USB CDC 要 Disabled**：这块板子下载和串口走的是 CH343（UART0，GPIO43/44）。如果开启 USB CDC，`Serial` 会输出到原生 USB（GPIO19/20），而 CH343 那个口什么都收不到，表现为"程序跑了但串口没输出"。
>
> 如果你把线插在**原生 USB 口**上调试，才需要改成 Enabled。

**驱动**：CH343 需要单独装驱动（WCH 官网 `CH343SER`），macOS 装完 `/dev/cu.usbmodem*` 或 `/dev/cu.wchusbserial*` 会出现。

**上传时若提示连接失败**：按住 BOOT → 点一下 RST → 松开 BOOT，进入下载模式再点上传。

---

## 7. 与项目接线表的核对

现有 `firmware/common/pins.h` 的分配**全部有效**，且落在这块板子上很顺：

| 功能 | 引脚 | 板上的位置 | 冲突检查 |
|---|---|---|---|
| 屏幕 MOSI/SCLK/CS/DC/RST/BL | 11/12/10/9/8/7 | 全在 J1（针 7、12、15、16、17、18） | ✅ 无冲突 |
| 麦克风 BCLK/WS/SD | 4/5/6 | J1 针 4–6 | ✅ |
| 功放 BCLK/LRC/DIN | 15/16/17 | J1 针 8–10 | ✅ |
| I2C SDA/SCL | 1/2 | J2 针 4–5 | ✅ |
| 按键 | 21 | J2 针 18 | ✅ |

**额外可用**：GPIO38–42、47 仍空闲，后续加按键、风扇、红外发射都可从这些里挑。

**新增注意**：
- 不要用 GPIO48 做别的（RGB 灯）。
- 插着 USB 线时不要用 19/20。
- 屏幕和功放都在 J1 一侧，接线时注意别把 5V（J1 针 21）和 3V3（J1 针 1/2）接反。

---

## 8. `data/` 资料包内容说明

| 文件/目录 | 内容 | 是否需要 |
|---|---|---|
| `ESP32-S3-SCH.pdf` | 原理图 V1.4（本档案主要依据） | 查阅用 |
| `ESP32-S3-Metric.pdf` / `-inch.pdf` | 尺寸图（mm / mil） | 开壳、画图用 |
| `ESP32-S3-WROOM系列型号对比.png` | 各型号 Flash/PSRAM 对照 | 已确认 N16R8 |
| `封面图.jpg` | 商品参数图 | 已提取参数 |
| `Arduino IDE软件搭建ESP32开发环境.txt` | 一个 CSDN 教程链接 | 可选 |
| `esp32s3蓝牙wifi代码.rar` | 3 个 Arduino 例程：WiFi 连接、BLE、RGB 灯 | ✅ 有用，已读 |
| `ESP32-S3开发板N16R8焊接.rar`（46MB） | MicroPython 例程、Thonny、Flash Download Tool、MicroPython 固件 | 本项目走 Arduino，**暂不需要** |
| `欣薇商城.url` | 店铺链接 | — |

> 46MB 压缩包里的内容是 **MicroPython 路线**的配套（固件 `YD-ESP32-S3-N16R8-MPY-V1.1.bin` + 例程 + Thonny）。我们走 Arduino，不用解压；哪天想转 MicroPython 再回头用。

---

## 9. 待确认事项

- [ ] 到手后核对丝印是否为 `ESP32-S3-WROOM-1-N16R8`
- [ ] 确认两个 USB-C 口的丝印标注（哪个是 COM / 哪个是 USB）
- [ ] 确认板子背面是否有 IPEX 天线座（影响外壳开孔）
- [ ] 实测板载 3V3 在屏幕全亮 + 功放工作时的电压跌落
