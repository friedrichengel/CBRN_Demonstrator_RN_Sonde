// ================================================================
// power.cpp - Deep-Sleep mit EXT0-Wake auf HW-Taste
// ================================================================
#include <Arduino.h>
#include <esp_sleep.h>
#include "power.h"
#include "config.h"
#include "display.h"
#include "colors.h"

void enterDeepSleep() {
  Serial.println("[PWR] -> Deep Sleep");

  gfx->fillScreen(COL_BG);
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(210, 95);
  gfx->print("Aus");
  gfx->setTextSize(1);
  gfx->setTextColor(COL_LGRAY);
  gfx->setCursor(115, 150);
  gfx->print("HW-Taste druecken zum Einschalten");
  gfx->flush();
  delay(800);

  output->displayOff();
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_HW, 0);
  esp_deep_sleep_start();
}
