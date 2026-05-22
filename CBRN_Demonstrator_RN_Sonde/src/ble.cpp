// ================================================================
// ble.cpp - NimBLE-Scan, Manufacturer-Data-Parsing
// ================================================================
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>
#include "ble.h"
#include "config.h"
#include "state.h"

Source sources[MAX_SOURCES];

const char* nuklid_name[9] = {
  "", "Cs-137", "Co-60", "Am-241", "Ir-192",
  "Ra-226", "Th-232", "U-238", "Pu-239"
};

static NimBLEScan* bleScan = nullptr;

class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    // Filter 1: Name muss mit "Strahler-" beginnen
    if (!dev->haveName()) return;
    std::string n = dev->getName();
    if (n.rfind("Strahler-", 0) != 0) return;

    // Filter 2: Manufacturer-Data Schema (Company 0xFFFF, Proto 0x01, CRC)
    if (!dev->haveManufacturerData()) return;
    std::string md = dev->getManufacturerData();
    if (md.length() < 9) return;
    if ((uint8_t)md[0] != 0xFF || (uint8_t)md[1] != 0xFF) return;
    if ((uint8_t)md[2] != 0x01) return;

    uint8_t crc = (uint8_t)md[2]^(uint8_t)md[3]^(uint8_t)md[4]^
                  (uint8_t)md[5]^(uint8_t)md[6]^(uint8_t)md[7];
    if (crc != (uint8_t)md[8]) return;

    uint8_t  sender_nr     = (uint8_t)md[3];
    uint8_t  nuklid        = (uint8_t)md[4];
    uint16_t max_dosis_100 = ((uint8_t)md[5] << 8) | (uint8_t)md[6];
    int8_t   tx_power      = (int8_t)md[7];
    int      rssi          = dev->getRSSI();

    uint32_t now = millis();

    int idx = -1;
    for (int i = 0; i < MAX_SOURCES; i++)
      if (sources[i].valid && sources[i].sender_nr == sender_nr) { idx = i; break; }

    if (idx < 0) {
      for (int i = 0; i < MAX_SOURCES; i++) if (!sources[i].valid) { idx = i; break; }
      if (idx < 0) return;
      sources[idx].valid          = true;
      sources[idx].bin_sum        = 0.0f;
      sources[idx].bin_count      = 0;
      sources[idx].bin_start_ms   = now;
      sources[idx].sec_buf_idx    = 0;
      sources[idx].sec_buf_count  = 0;
      sources[idx].rssi_smooth    = (float)rssi;   // Sofort-Anzeige bis Bin abgeschlossen
      sources[idx].rssi_peak      = (float)rssi;
      strncpy(sources[idx].name, n.c_str(), 19);
      sources[idx].name[19] = 0;
    }

    Source &s        = sources[idx];
    s.sender_nr      = sender_nr;
    s.nuklid         = nuklid;
    s.max_dosis_100  = max_dosis_100;
    s.tx_power       = tx_power;
    s.last_seen      = now;

    // Stufe 1: in aktuelle Sekunden-Bin akkumulieren
    s.bin_sum   += (float)rssi;
    s.bin_count += 1;

    // Stufe 2: Bin abgeschlossen? -> Mittelwert in Sek-Ringbuffer, dann
    // Moving Average ueber die letzten RSSI_AVG_SECS Sekunden-Werte
    if ((now - s.bin_start_ms) >= RSSI_BIN_MS && s.bin_count > 0) {
      float bin_mean = s.bin_sum / s.bin_count;
      s.sec_buf[s.sec_buf_idx] = bin_mean;
      s.sec_buf_idx = (s.sec_buf_idx + 1) % RSSI_AVG_SECS;
      if (s.sec_buf_count < RSSI_AVG_SECS) s.sec_buf_count++;

      float sum = 0.0f;
      for (uint8_t i = 0; i < s.sec_buf_count; i++) sum += s.sec_buf[i];
      s.rssi_smooth = sum / s.sec_buf_count;

      if (s.rssi_smooth > s.rssi_peak) s.rssi_peak = s.rssi_smooth;

      s.bin_sum      = 0.0f;
      s.bin_count    = 0;
      s.bin_start_ms = now;
    }
  }
};
static ScanCB scanCallbacks;

static void startScan() {
  if (!bleScan) bleScan = NimBLEDevice::getScan();
  bleScan->stop();
  bleScan->setScanCallbacks(&scanCallbacks, true);
  bleScan->setActiveScan(false);
  // Duty ~50% (Window 50 / Interval 100) -> nimble_host blockiert
  // IDLE0 nicht mehr, WDT bleibt zufrieden.
  bleScan->setInterval(100);
  bleScan->setWindow(50);

#if CONFIG_BT_NIMBLE_EXT_ADV
  // PHY-Auswahl:
  //   Normal -> 1M (Legacy Advertising, kompatibel mit BLE 4+)
  //   LR     -> Coded PHY (BLE 5 Extended Advertising, S=8, lange Reichweite)
  bleScan->setPhy(currentBleMode == BLE_LR
                  ? NimBLEScan::SCAN_CODED
                  : NimBLEScan::SCAN_1M);
#endif

  bleScan->start(0, false);
}

void initBle() {
  memset(sources, 0, sizeof(sources));
  NimBLEDevice::init("Sonde");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  startScan();
  Serial.println("[BLE] NimBLE scan running");
}

void switchBleMode(BleMode newMode) {
  if (newMode == currentBleMode) return;
  currentBleMode = newMode;
  startScan();
  Serial.printf("[BLE] Mode -> %s\n",
                newMode == BLE_LR ? "LR/Coded" : "Normal/1M");
}
