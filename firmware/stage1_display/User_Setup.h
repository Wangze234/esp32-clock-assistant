// ============================================================
// TFT_eSPI  User_Setup.h
// 屏幕：MSP2807  2.8" 320x240  ILI9341 + XPT2046 电阻触摸
// 主控：YD-ESP32-S3 N16R8
// 项目：esp32-clock-assistant / 阶段 1
// 生成：2026-11-06
//
// 用法：备份库里的原文件后，用本文件覆盖
//       ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
// ============================================================

#define USER_SETUP_INFO "MSP2807_ESP32S3"

// ---------- 驱动 ----------
#define ILI9341_DRIVER
// 注意：本屏是 ILI9341，不是 ST7789

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---------- 显示 SPI 引脚 ----------
#define TFT_MISO 13        // 实际接触摸 TP_OUT，显示本身不需要读
#define TFT_MOSI 11        // 同时并联 TP_DIN
#define TFT_SCLK 12        // 同时并联 TP_CLK
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8

// ---------- 背光 ----------
#define TFT_BL    7
#define TFT_BACKLIGHT_ON HIGH   // 板载三极管，高电平点亮

// ---------- 触摸 ----------
#define TOUCH_CS 14
#define SPI_TOUCH_FREQUENCY 2500000

// ---------- SPI 速度 ----------
#define SPI_FREQUENCY       40000
#define SPI_READ_FREQUENCY  20000
#define SPI_WRITE_FREQUENCY 40000

// ---------- 字体 ----------
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ---------- 可选修正（默认注释，出问题时再打开） ----------
// 红蓝反了（红色显示成蓝色）时打开：
//#define TFT_RGB_ORDER TFT_BGR

// 颜色整体反相（黑白颠倒）时打开：
//#define TFT_INVERSION_ON

// 花屏时把 SPI_FREQUENCY 降到 27000 试试
