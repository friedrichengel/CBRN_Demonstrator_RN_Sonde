// ================================================================
// dose.cpp - Dose-Falloff + Auto-Range
// ================================================================
#include <math.h>
#include "dose.h"
#include "config.h"
#include "colors.h"

float calcDose(const Source &s) {
  // delta_db immer <= 0 (peak ist Best-RSSI-je-gesehen)
  float delta_db = s.rssi_smooth - s.rssi_peak;
  if (delta_db > 0.0f) delta_db = 0.0f;
  // Pfadverlust -> Distanz -> Abstandsquadratgesetz
  // I = I_max * 10^(delta / (5*n))
  float max_dose = s.max_dosis_100 / 100.0f;
  return max_dose * powf(10.0f, delta_db / (5.0f * DOSE_PATH_LOSS_N));
}

uint16_t colorForFrac(float frac) {
  if (frac < 0.50f) return COL_GREEN;
  if (frac < 0.75f) return COL_YELLOW;
  if (frac < 0.90f) return COL_ORANGE;
  return COL_RED;
}

// Skala wird durch max_dose_usvh (= Vollausschlag der Quelle) bestimmt.
// Einheit (nSv/uSv/mSv) wird so gewaehlt, dass der Vollausschlag lesbar ist.
RangeResult autoRange(float dose_usvh, float max_dose_usvh) {
  if (max_dose_usvh <= 0.0f) max_dose_usvh = 1.0f;
  float frac = dose_usvh / max_dose_usvh;
  uint16_t c = colorForFrac(frac);

  if (max_dose_usvh < 1.0f) {
    return {dose_usvh * 1000.0f, max_dose_usvh * 1000.0f, "nSv/h", c};
  } else if (max_dose_usvh < 1000.0f) {
    return {dose_usvh, max_dose_usvh, "uSv/h", c};
  } else {
    return {dose_usvh / 1000.0f, max_dose_usvh / 1000.0f, "mSv/h", c};
  }
}
