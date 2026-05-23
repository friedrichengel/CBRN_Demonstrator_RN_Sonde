// ================================================================
// dose.h - Dosis-Berechnung + Auto-Range fuer Gauge-Anzeige
// ================================================================
#pragma once
#include <stdint.h>
#include "ble.h"

struct RangeResult {
  float       display_val;
  float       gauge_max;
  const char* unit;
  uint16_t    color;
};

float       calcDose(const Source& s);
uint16_t    colorForFrac(float frac);
RangeResult autoRange(float dose_usvh, float max_dose_usvh);

// True solange der RSSI-Sekunden-Ringbuffer noch nicht voll ist
// (nach Erstkontakt oder Wiederfund). Dose-Wert ist in dieser Zeit noch
// nicht aussagekraeftig - Anzeige sollte "Kalibrierung..." zeigen.
bool        isCalibrating(const Source& s);
