/*
 * ============================================================
 * 阶段 0：板子到货自检
 * 主控：YD-ESP32-S3 N16R8（ESP32-S3-WROOM-1-N16R8）
 * ============================================================
 *
 * 目的：确认芯片型号、Flash 16MB、PSRAM 8MB 都对。
 * 不接任何外设，只插 USB 线即可。
 *
 * 烧录后：串口监视器 115200，按 RST，应看到
 *   Chip      : ESP32-S3 rev 0, 2 cores
 *   Flash     : 16 MB
 *   PSRAM     : 8 MB (free 8192 KB)
 * ============================================================
 */

#include <Arduino.h>

static void printSelfCheck() {
  Serial.println();
  Serial.println("===== Board Self Check =====");
  Serial.printf("Chip      : %s rev %d, %d cores\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores());
  Serial.printf("CPU       : %d MHz\n", getCpuFrequencyMhz());
  Serial.printf("Flash     : %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("PSRAM     : %d MB (free %d KB)\n",
                ESP.getPsramSize() / (1024 * 1024), ESP.getFreePsram() / 1024);
  Serial.printf("Heap      : %d KB free\n", ESP.getFreeHeap() / 1024);
  Serial.printf("Uptime    : %lu s\n", millis() / 1000);
  Serial.println("============================");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  printSelfCheck();
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    printSelfCheck();
  }
  delay(100);
}
