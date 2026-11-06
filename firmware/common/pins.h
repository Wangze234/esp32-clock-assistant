// pins.h — 全项目统一引脚定义
// 必须与 README.md §3.1 接线表保持一致。
// 修改前先更新 README，避免文档和代码脱节。
//
// 目标开发板：YD-ESP32-S3 N16R8（ESP32-S3-WROOM-1-N16R8，16MB Flash + 8MB Octal PSRAM）
// 板卡档案：hardware/board-esp32s3-n16r8.md
// 已避开：GPIO 19/20 (原生 USB)、43/44 (CH343 UART0)、33-37 (Octal PSRAM 占用)、
//         45/46 (strapping)、0 (BOOT)、48 (板载 WS2812 RGB)

#pragma once

// ---------- 2.8" SPI 屏 MSP2807 (ILI9341) ----------
#define PIN_TFT_MOSI   11
#define PIN_TFT_SCLK   12
#define PIN_TFT_MISO   13   // 仅供触摸 TP_OUT 使用，显示不需要读
#define PIN_TFT_CS     10
#define PIN_TFT_DC      9
#define PIN_TFT_RST     8
#define PIN_TFT_BL      7

// ---------- XPT2046 电阻触摸（与屏共用 SPI） ----------
// TP_CLK 并到 PIN_TFT_SCLK，TP_DIN 并到 PIN_TFT_MOSI
#define PIN_TOUCH_CS   14
#define PIN_TOUCH_IRQ  47   // 可选


// ---------- INMP441 麦克风 (I2S RX) ----------
#define PIN_MIC_BCLK    4
#define PIN_MIC_WS      5
#define PIN_MIC_SD      6

// ---------- MAX98357A 功放 (I2S TX) ----------
#define PIN_AMP_BCLK   15
#define PIN_AMP_LRC    16
#define PIN_AMP_DIN    17

// ---------- I2C 总线 (AHT20 0x38 / DS3231 0x68) ----------
#define PIN_I2C_SDA     1
#define PIN_I2C_SCL     2

// ---------- 用户按键 (内部上拉，按下接地) ----------
#define PIN_BUTTON     21
