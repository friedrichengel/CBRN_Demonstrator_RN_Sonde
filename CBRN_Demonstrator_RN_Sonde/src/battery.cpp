// ================================================================
// battery.cpp - ADC-Sampling + Lade-Erkennung
// ================================================================
#include <Arduino.h>
#include "battery.h"
#include "config.h"

BatState bat = { 3.7f, 50, false };

static uint32_t batLastSampleMs = 0;

static float readBatteryVoltage() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_BAT_VOLT);
  return ((sum / 8) * 2.0f) / 1000.0f;   // Spannungsteiler 1:2
}

static int batteryPercent(float v) {
  if (v >= 4.15f) return 100;
  if (v <= 3.30f) return 0;
  return (int)((v - 3.30f) / 0.85f * 100.0f);
}

void updateBattery() {
  uint32_t now = millis();
  if (now - batLastSampleMs < BAT_SAMPLE_MS) return;
  batLastSampleMs = now;

  float v = readBatteryVoltage();
  bat.voltage  = bat.voltage * 0.7f + v * 0.3f;
  bat.percent  = batteryPercent(bat.voltage);
  // TP4056 ohne CHRG-Pin am ESP -> Heuristik: Spannung >= 4.15V = USB anliegt
  bat.charging = bat.voltage >= 4.15f;
}
