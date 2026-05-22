// ================================================================
// ble.h - BLE-Scan (NimBLE) und Quellen-Datenmodell
// ================================================================
#pragma once
#include <stdint.h>
#include "state.h"
#include "config.h"

struct Source {
  bool     valid;
  uint8_t  sender_nr;
  uint8_t  nuklid;
  uint16_t max_dosis_100;   // 0.01 uSv/h Einheiten
  int8_t   tx_power;
  // Stufe 1: aktueller 1-Sek-Bin (kumuliert)
  float    bin_sum;
  uint16_t bin_count;
  uint32_t bin_start_ms;
  // Stufe 2: Ringbuffer der Sekunden-Mittelwerte
  float    sec_buf[RSSI_AVG_SECS];
  uint8_t  sec_buf_idx;
  uint8_t  sec_buf_count;
  // Berechnete Mittelwerte (Update nur bei Bin-Abschluss)
  float    rssi_smooth;
  float    rssi_peak;
  uint32_t last_seen;
  char     name[20];
};

extern Source sources[MAX_SOURCES];
extern const char* nuklid_name[9];

void initBle();
void switchBleMode(BleMode newMode);
