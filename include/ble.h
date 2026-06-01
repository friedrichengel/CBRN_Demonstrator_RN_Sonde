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
  float    max_dose_usvh;   // Vollausschlag der Quelle in uSv/h (aus Roh-Wert + Einheits-Index konvertiert)
  uint8_t  unit_index;      // 0=nSv/h, 1=uSv/h, 2=mSv/h, 3=Sv/h (wie vom Sender gesendet)
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
void stopBle();   // NimBLE deinitialisieren (gibt Radio/Heap fuer WLAN frei)
void switchBleMode(BleMode newMode);
