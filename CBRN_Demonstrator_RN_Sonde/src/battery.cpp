// ================================================================
// battery.cpp - ADC-Sampling + Lade-Erkennung
// ================================================================
#include <Arduino.h>
#include "battery.h"
#include "config.h"

extern "C" {
  #include "soc/usb_reg.h"   // USB-OTG Register (GOTGCTL fuer VBUS-Detection)
}

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

// ESP32-S3 USB-OTG: GOTGCTL Bit 19 (BSesVld) = VBUS valid.
// Funktioniert auch fuer reine USB-Netzteile ohne Host-Enumeration.
// Voraussetzung: USB-Peripheral getaktet -> durch ARDUINO_USB_CDC_ON_BOOT=1
// (siehe platformio.ini) erfuellt.
static bool readVbusPresent() {
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  return (REG_READ(USB_GOTGCTL_REG) & (1U << 19)) != 0;
#else
  return false;
#endif
}

void updateBattery() {
  uint32_t now = millis();
  if (now - batLastSampleMs < BAT_SAMPLE_MS) return;
  batLastSampleMs = now;

  float v = readBatteryVoltage();
  bat.voltage  = bat.voltage * 0.7f + v * 0.3f;
  bat.percent  = batteryPercent(bat.voltage);

  // 1) Hardware: USB-OTG VBUS-Bit -> zuverlaessig, sobald USB steckt
  // 2) Fallback: Spannung >= 4.18 V (TP4056 CV-Phase, falls Register-Lesung
  //    aus irgendeinem Grund nicht greift)
  bat.charging = readVbusPresent() || (bat.voltage >= 4.18f);
}
