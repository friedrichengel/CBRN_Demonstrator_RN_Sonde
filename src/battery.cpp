// ================================================================
// battery.cpp - ADC-Sampling + Lade-Erkennung
//
// VBUS/Lade-Detection laeuft mehrgleisig (OR-verknuepft):
//   1) HW-Pin PIN_VBUS_DET (wenn >= 0)
//      - INPUT_PULLDOWN; unverloetet -> immer LOW -> kein Einfluss
//      - mit Loetmod (10k/10k Teiler von USB-VBUS) -> sofortiges HIGH
//   2) SW-Heuristik:
//      a) Spannungsanstieg > 5 mV ueber 30 s -> charging
//      b) Spannungsabfall < -5 mV ueber 30 s -> nicht charging
//      c) Hysterese sonst (Zustand bleibt)
//   3) Absolute Schwelle: Spannung >= 4.18 V -> charging
//      (CV-Phase / Erhaltungsladung)
// ================================================================
#include <Arduino.h>
#include "battery.h"
#include "config.h"

BatState bat = { 3.7f, 50, false };

static uint32_t batLastSampleMs = 0;

// SW-Heuristik State
static float    swVRef     = 0.0f;
static uint32_t swVRefStamp = 0;
static bool     swCharging  = false;

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

void initBattery() {
#if PIN_VBUS_DET >= 0
  // Pulldown -> unverloetet liest der Pin sicher LOW (kein Floating-Noise).
  // Externer 10k/10k-Teiler ueberstimmt den ~50k internen Pulldown problemlos.
  pinMode(PIN_VBUS_DET, INPUT_PULLDOWN);
#endif
}

void updateBattery() {
  uint32_t now = millis();
  if (now - batLastSampleMs < BAT_SAMPLE_MS) return;
  batLastSampleMs = now;

  float v = readBatteryVoltage();
  bat.voltage  = bat.voltage * 0.7f + v * 0.3f;
  bat.percent  = batteryPercent(bat.voltage);

  // SW-Heuristik: Spannungsanstieg ueber 30s-Fenster
  if (swVRefStamp == 0) {
    swVRef      = bat.voltage;
    swVRefStamp = now;
  } else if (now - swVRefStamp >= 30000) {
    float delta = bat.voltage - swVRef;
    if      (delta >  0.005f) swCharging = true;   // klar steigend -> Laden
    else if (delta < -0.005f) swCharging = false;  // klar fallend  -> Akku-Betrieb
    // sonst: Zustand bleibt (Hysterese gegen ADC-Rauschen)
    swVRef      = bat.voltage;
    swVRefStamp = now;
  }

  // HW-Pin (wenn verdrahtet) + SW-Heuristik + absolute Schwelle, OR-verknuepft
  bool hw_vbus = false;
#if PIN_VBUS_DET >= 0
  hw_vbus = digitalRead(PIN_VBUS_DET) == HIGH;
#endif
  bat.charging = hw_vbus || swCharging || (bat.voltage >= 4.18f);
}
