# 阶段 0：环境搭建（macOS 逐步指南）

> 目标：板子到货之前，把电脑上的开发环境全部准备好。
> 预计耗时：40–60 分钟（下载速度决定）。
> 全程不需要连接开发板。

---

## 验收标准

全部打勾才算阶段 0 完成：

- [ ] 系统能识别 CH343 串口（或确认走原生 USB 的方案）
- [ ] Arduino IDE 安装完成，能正常打开
- [ ] 开发板管理器里装好 `esp32` 支持包
- [ ] 能选到 `ESP32S3 Dev Module` 并设置好参数
- [ ] 库管理器里能看到 `TFT_eSPI`
- [ ] 能成功编译一个 Blink 示例（不要求上传）

---

## 步骤 1：安装 CH343 串口驱动

这块板子的下载口是 CH343P，macOS 不自带驱动，必须先装。

### 1.1 下载

1. 打开 WCH 官网下载页：<https://www.wch.cn/downloads/>
2. 搜索 **CH343**，找到 **CH343SER**（USB 转串口驱动）
3. 下载 **macOS 版本**（文件名通常含 `MAC`，是一个 zip）
4. 解压后双击里面的 `.pkg` 安装包

### 1.2 允许系统扩展（macOS 13 以后必做）

安装完成后，打开：

**系统设置 → 隐私与安全性**，往下翻到"安全性"区域

如果看到类似 `WCH` / `CH34x` 的提示，点 **"允许"**，然后**重启电脑**。

> 不点允许，驱动装了也不会生效，串口列表里看不到设备。

### 1.3 验证驱动是否生效

打开终端，执行：

```bash
ls /dev/cu.*
```

**现在还没插板子**，所以看不到 CH343 设备。可以这样确认驱动已加载：

```bash
systemextensionsctl list | grep -i -E "wch|ch34"
```

有输出（哪怕显示 `activated`）就说明驱动装上了。如果完全没有输出，回到 1.2 检查系统扩展是否被拦截。

> 板子到货后，插上 COM 口再执行 `ls /dev/cu.*`，应该出现 `/dev/cu.wchusbserial*` 或 `/dev/cu.usbserial-*`。

### 1.4 如果驱动实在装不上：备用方案

这块板子有两个 USB-C 口，另一个是**原生 USB**，ESP32-S3 自带 USB 控制器，**不需要任何驱动**。

备用方案：
1. 把数据线插到**另一个 USB-C 口**（原生 USB）
2. Arduino IDE 里把 `USB CDC On Boot` 改成 **Enabled**
3. 端口会显示为 `/dev/cu.usbmodem*`

这个方案完全可行，只是串口行为和 CH343 略有不同。**先用 CH343，装不上再切。**

---

## 步骤 2：安装 Arduino IDE

1. 打开 <https://www.arduino.cc/en/software>
2. 下载 **macOS** 版本
   - Apple Silicon（M1/M2/M3/M4）选 **Apple Silicon**
   - Intel 芯片选 **Intel**
3. 打开 dmg，把 Arduino IDE 拖进「应用程序」
4. 首次打开如果提示"无法验证开发者"：右键 → 打开，或去「隐私与安全性」点"仍要打开"

验证：能正常打开 IDE，界面语言可在 `Arduino IDE → Settings → Language` 改成中文。

---

## 步骤 3：添加开发板管理器地址

1. 打开 **Arduino IDE → Settings**（快捷键 `⌘ ,`）
2. 找到 **Additional boards manager URLs**（附加开发板管理器网址）
3. 点右侧图标，粘贴这一行：

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

4. 点 OK 保存

> 如果这一栏已经有其他地址，用逗号或换行分隔，不要删掉原来的。

---

## 步骤 4：安装 ESP32 支持包

1. 左侧栏点 **Boards Manager**（开发板管理器，积木图标）
2. 搜索 `esp32`
3. 找到 **esp32 by Espressif Systems**，点 **Install**
4. 等待下载完成（约 300MB–1GB，视版本而定）

**装哪个版本？** 选列表里的最新稳定版（3.x）。如果后续 TFT_eSPI 编译报错，再回退到 2.0.17。

### 国内网络下载慢/失败怎么办

这是阶段 0 最常见的卡点。三个办法，按顺序试：

1. **挂代理**：Arduino IDE → Settings → Network，填入 HTTP 代理地址
2. **换时间段重试**：下载支持断点续传，失败后重装通常能接着下
3. **用手机热点**：有时运营商线路差异很大

不要从第三方站点随便下 zip 包，版本对不上会导致更多问题。

---

## 步骤 5：选择开发板和参数

> **选项在哪里？** 在 macOS 屏幕**最顶部**的菜单栏里点「工具 / Tools」。
> 注意不是 Arduino 窗口内部，macOS 的菜单栏是全局的，只有 IDE 处于前台时才显示。
> 选好开发板后，重新点开「工具」，菜单**下半部分**才会出现 PSRAM、Flash Size、USB CDC 这些选项。
> 如果「工具」里只有 Board / Port / Get Board Info 三项，说明开发板还没选上，先做第 1 步。

1. 菜单 **Tools → Board → esp32 → ESP32S3 Dev Module**
2. 按下面这张表逐项设置（`Tools` 菜单里）：

| 菜单项 | 设置值 |
|---|---|
| Board | **ESP32S3 Dev Module** |
| USB CDC On Boot | **Disabled** |
| CPU Frequency | 240MHz (WiFi) |
| Flash Mode | QIO 80MHz |
| Flash Size | **16MB (128Mb)** |
| Partition Scheme | **16M Flash (3MB APP/9.9MB FATFS)**（列表里没有就先用 2MB APP/12.5MB，阶段 5 前再换） |
| PSRAM | **OPI PSRAM** |
| Upload Speed | 921600 |
| JTAG Adapter | Disabled |

**两个最容易选错的：**

- **PSRAM 必须选 `OPI PSRAM`**。这块板子是 N16R8，8MB PSRAM 走 Octal 八线模式。选 `QSPI PSRAM` 或 `Disabled`，后面语音阶段会直接内存不足。
- **USB CDC On Boot 选 `Disabled`**。因为下载走 CH343（UART0）。选了 Enabled，`Serial` 会输出到原生 USB，你在 CH343 的串口监视器里什么都看不到。

> 如果走步骤 1.4 的原生 USB 方案，则反过来：USB CDC On Boot = **Enabled**。

---

## 步骤 6：安装库

打开 **Library Manager**（库管理器，书本图标），逐个搜索并安装：

| 库名 | 用途 | 什么时候用 |
|---|---|---|
| **TFT_eSPI** | 驱动 SPI 屏幕 | 阶段 1 必装 |
| **ArduinoJson** | 解析天气 API | 阶段 3 |
| **Adafruit AHTX0** | AHT20 温湿度 | 阶段 4 |
| **RTClib** | DS3231 RTC | 阶段 4 |

阶段 0 只需要装 **TFT_eSPI**，其余可以以后装。搜索时认准作者：

- TFT_eSPI → **Bodmer**
- ArduinoJson → **Benoit Blanchon**

> ⚠️ 现在**不要**去改 TFT_eSPI 的 `User_Setup.h`，那是阶段 1 的事。先装好即可。

---

## 步骤 7：编译验证（不需要板子）

这一步证明工具链是通的。

1. 菜单 **File → Examples → 01.Basics → Blink**
2. 确认开发板仍然是 `ESP32S3 Dev Module`
3. 点左上角 **Verify**（对勾图标，快捷键 `⌘ R`）
4. 等待编译完成

**成功标志：** 底部输出 `Compiling sketch...` 后出现

```
Sketch uses xxxxxx bytes (x%) of program storage space.
Global variables use xxxxx bytes ...
```

**不需要点上传**（现在也没插板子）。

---

## 步骤 8：板子到货后的首次验证（阶段 1 的入口）

到货后做这一步，一次性确认板子、驱动、PSRAM、Flash 都正常。

### 8.1 连接

1. 数据线插到板子的 **COM 口**（CH343 那个）
2. 终端执行 `ls /dev/cu.*`，确认出现 `/dev/cu.wchusbserial*`
3. Arduino IDE → **Tools → Port**，选择该端口
4. 如果端口列表是空的，回到步骤 1 检查驱动

> ⚠️ **不要选 `/dev/cu.debug-console`**。这是 macOS 系统自带的虚拟调试串口，不是开发板。选它上传必然失败。
> 正确的端口特征：`/dev/cu.wchusbserial*`（CH343）或 `/dev/cu.usbmodem*`（原生 USB）。
> 板子没插时，Port 列表里只会出现 `debug-console` 这类系统端口，属于正常现象。

### 8.2 烧录自检程序

新建 Sketch，粘贴以下代码：

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("===== Board Self Check =====");
  Serial.printf("Chip      : %s rev %d, %d cores\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
  Serial.printf("CPU       : %d MHz\n", getCpuFrequencyMhz());
  Serial.printf("Flash     : %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("PSRAM     : %d MB (free %d KB)\n",
                ESP.getPsramSize() / (1024 * 1024), ESP.getFreePsram() / 1024);
  Serial.printf("Free heap : %d KB\n", ESP.getFreeHeap() / 1024);
  Serial.println("============================");
}

void loop() {
  delay(1000);
}
```

点 **Upload**（右箭头 `→`）。

### 8.3 期望输出

打开 **Serial Monitor**（右上角放大镜图标），波特率选 **115200**，按一下板子的 RST 键，应该看到：

```
===== Board Self Check =====
Chip      : ESP32-S3 rev 0, 2 cores
CPU       : 240 MHz
Flash     : 16 MB
PSRAM     : 8 MB (free 8192 KB)
Free heap : 3xx KB
============================
```

**必须对上这三个数：**

| 项目 | 期望值 | 不对说明 |
|---|---|---|
| Flash | 16 MB | 分区/Flash Size 选错 |
| PSRAM | 8 MB | **PSRAM 没选 OPI PSRAM** |
| Chip | ESP32-S3 | 开发板选错 |

如果 PSRAM 显示 0 MB 或 `free 0 KB`，先检查 `Tools → PSRAM` 设置，再检查是不是买到了 N8R2 的板子。

---

## 常见问题排查

| 现象 | 可能原因 | 解决 |
|---|---|---|
| `ls /dev/cu.*` 没有新设备 | 驱动没装/没允许；线是充电线 | 重装驱动并在隐私与安全性里允许；换一根**数据线** |
| Port 菜单是灰的 | 同上 | 同上；换 USB 口试 |
| 上传报 `Failed to connect to ESP32-S3` | 没进下载模式 | 按住 **BOOT** → 点一下 **RST** → 松开 BOOT，再点上传 |
| 上传报 `No serial data received` | 波特率太高/线太长 | Upload Speed 降到 460800 或 115200 |
| 上传报 `Could not open port` | 串口监视器还开着 | 关掉 Serial Monitor 再上传 |
| 串口输出乱码 | 波特率不匹配 | Serial Monitor 改成 115200 |
| 程序跑起来但串口无输出 | USB CDC On Boot 选错 | 用 CH343 时选 Disabled |
| 编译报 PSRAM 相关错误 | PSRAM 选错 | 改成 OPI PSRAM |
| TFT_eSPI 编译报错 | 库版本与 core 3.x 不兼容 | 升级 TFT_eSPI 到最新版 |
| 开发板管理器下载失败 | 国内网络 | 挂代理 / 换时段重试 |

---

## 完成记录

做完后回填：

| 项目 | 结果 |
|---|---|
| 完成日期 | 2026-11-08 |
| CH343 驱动版本 | `cn.wch.CH34xVCPDriver 1.0`（已 activated enabled）；串口 `/dev/cu.wchusbserial5D4D0989291` |
| Arduino IDE 版本 | 2.3.10（另有内置 arduino-cli 1.5.1） |
| esp32 支持包版本 | 3.3.12 |
| 编译 Blink 是否通过 | ✅ |
| 板子到货后自检输出 | ESP32-S3 rev 2 / 2 cores / 240MHz / Flash 16MB / PSRAM 8MB (free 8188KB) / Heap 348KB |

> 完成后告诉我，我会更新 README 的 §0 状态和 §8 日志，然后进入阶段 1：点亮屏幕。
