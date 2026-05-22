// ================================================================
// CBRN-RN Trainingsgerät – Sonden-Firmware v1.0
// Hardware: LilyGO T-Display-S3 AMOLED (ESP32-S3, RM67162, 536x240)
//
// Funktion:
//   - Scannt BLE-Advertising-Pakete der Strahler
//   - Findet den Strahler mit bestem RSSI
//   - Berechnet simulierte Dosisleistung
//   - Zeigt Analogzeiger + digitale Anzeige mit Auto-Range
//   - Debug-Flag: zeigt Strahler-Name + RSSI aller Quellen
//
// Dosismodell:
//   Dosis = max_dosis * 10^((RSSI_now - RSSI_peak) / 10)
//   RSSI_peak = bester je gesehener RSSI dieser Quelle
//
// Auto-Range:
//   < 1 µSv/h  → nSv/h Skala (0-1000 nSv/h)
//   < 1 mSv/h  → µSv/h Skala (0-1000 µSv/h)
//   >= 1 mSv/h → mSv/h Skala (0-10 mSv/h)
//
// DEBUG_MODE true: Strahler-Name + RSSI im unteren Bereich
// ================================================================

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Arduino_GFX_Library.h>
#include <math.h>

// ----------------------------------------------------------------
// Debug-Flag
// ----------------------------------------------------------------
#define DEBUG_MODE  false

// ----------------------------------------------------------------
// Display (RM67162, QSPI, 536x240)
// ----------------------------------------------------------------
#define QSPI_CS   6
#define QSPI_SCK  47
#define QSPI_D0   18
#define QSPI_D1    7
#define QSPI_D2   48
#define QSPI_D3    5
#define TFT_RST   17

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  QSPI_CS, QSPI_SCK, QSPI_D0, QSPI_D1, QSPI_D2, QSPI_D3);
Arduino_GFX *gfx = new Arduino_RM67162(bus, TFT_RST, 0);

// Farben
#define COL_BG     0x0000
#define COL_GREEN  0x07E0
#define COL_YELLOW 0xFFE0
#define COL_ORANGE 0xFD20
#define COL_RED    0xF800
#define COL_WHITE  0xFFFF
#define COL_LGRAY  0xC618
#define COL_DGRAY  0x4208
#define COL_CYAN   0x07FF

// Nuklid-Namen
const char* nuklid_name[9] = {
  "", "Cs-137", "Co-60", "Am-241", "Ir-192",
  "Ra-226", "Th-232", "U-238", "Pu-239"
};

// ----------------------------------------------------------------
// Strahler-Daten
// ----------------------------------------------------------------
#define MAX_SOURCES       16
#define SOURCE_TIMEOUT_MS 5000
#define RSSI_SMOOTH       0.3f

struct Source {
  bool     valid;
  uint8_t  sender_nr;
  uint8_t  nuklid;
  uint16_t max_dosis_100;
  int8_t   tx_power;
  float    rssi_smooth;
  float    rssi_peak;
  uint32_t last_seen;
  char     name[20];
};
Source sources[MAX_SOURCES];

// ----------------------------------------------------------------
// BLE Scan-Callback
// ----------------------------------------------------------------
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    if (!dev.haveManufacturerData()) return;
    std::string md = dev.getManufacturerData();
    if (md.length() < 9) return;
    if ((uint8_t)md[0] != 0xFF || (uint8_t)md[1] != 0xFF) return;
    if ((uint8_t)md[2] != 0x01) return;

    // CRC prüfen
    uint8_t crc = (uint8_t)md[2]^(uint8_t)md[3]^(uint8_t)md[4]^
                  (uint8_t)md[5]^(uint8_t)md[6]^(uint8_t)md[7];
    if (crc != (uint8_t)md[8]) return;

    uint8_t  sender_nr     = (uint8_t)md[3];
    uint8_t  nuklid        = (uint8_t)md[4];
    uint16_t max_dosis_100 = ((uint8_t)md[5]<<8)|(uint8_t)md[6];
    int8_t   tx_power      = (int8_t)md[7];
    int      rssi          = dev.getRSSI();

    // Quelle suchen oder neu anlegen
    int idx = -1;
    for (int i = 0; i < MAX_SOURCES; i++) {
      if (sources[i].valid && sources[i].sender_nr == sender_nr) {
        idx = i; break;
      }
    }
    if (idx < 0) {
      for (int i = 0; i < MAX_SOURCES; i++) {
        if (!sources[i].valid) { idx = i; break; }
      }
      if (idx < 0) return;
      sources[idx].valid       = true;
      sources[idx].rssi_smooth = (float)rssi;
      sources[idx].rssi_peak   = (float)rssi;
      if (dev.haveName()) {
        strncpy(sources[idx].name, dev.getName().c_str(), 19);
        sources[idx].name[19] = 0;
      } else {
        snprintf(sources[idx].name, 20, "Q%03d", sender_nr);
      }
    }

    Source &s        = sources[idx];
    s.sender_nr      = sender_nr;
    s.nuklid         = nuklid;
    s.max_dosis_100  = max_dosis_100;
    s.tx_power       = tx_power;
    s.last_seen      = millis();
    s.rssi_smooth    = s.rssi_smooth*(1.0f-RSSI_SMOOTH)+(float)rssi*RSSI_SMOOTH;
    if (s.rssi_smooth > s.rssi_peak) s.rssi_peak = s.rssi_smooth;
  }
};

// ----------------------------------------------------------------
// Dosisberechnung (µSv/h)
// ----------------------------------------------------------------
float calcDose(const Source &s) {
  float delta_db  = s.rssi_smooth - s.rssi_peak;
  float max_dose  = s.max_dosis_100 / 100.0f;
  return max_dose * pow(10.0f, delta_db / 10.0f);
}

// ----------------------------------------------------------------
// Auto-Range
// ----------------------------------------------------------------
struct RangeResult {
  float       display_val;   // Wert in der Anzeigeeinheit
  float       gauge_max;     // Vollausschlag der Skala
  const char* unit;
  uint16_t    color;
};

RangeResult autoRange(float dose_usvh) {
  if (dose_usvh < 1.0f) {
    // nSv/h Skala
    float v = dose_usvh * 1000.0f;
    uint16_t c = (v < 300) ? COL_GREEN :
                 (v < 700) ? COL_YELLOW : COL_ORANGE;
    return {v, 1000.0f, "nSv/h", c};
  } else if (dose_usvh < 1000.0f) {
    // µSv/h Skala
    uint16_t c = (dose_usvh < 100) ? COL_YELLOW :
                 (dose_usvh < 500) ? COL_ORANGE : COL_RED;
    return {dose_usvh, 1000.0f, "uSv/h", c};
  } else {
    // mSv/h Skala
    float v = dose_usvh / 1000.0f;
    return {v, 10.0f, "mSv/h", COL_RED};
  }
}

// ----------------------------------------------------------------
// Gauge-Zeichnung (Halbkreis, unten mittig)
// ----------------------------------------------------------------
#define GCX     268    // Zentrum X
#define GCY     225    // Zentrum Y
#define GR      165    // Außenradius Skala
#define GR_TXT  130    // Radius Beschriftung
#define GR_NDL  150    // Nadellänge

// Winkel: 210° (links/0%) bis -30° (rechts/100%) = 240° Bereich
float gaugeAngle(float fraction) {
  return 210.0f - constrain(fraction,0.0f,1.0f) * 240.0f;
}

void drawNeedle(float fraction, uint16_t col) {
  float rad = gaugeAngle(fraction) * M_PI / 180.0f;
  int x2 = GCX + (int)(GR_NDL * cos(rad));
  int y2 = GCY - (int)(GR_NDL * sin(rad));
  gfx->drawLine(GCX,   GCY,   x2,   y2,   col);
  gfx->drawLine(GCX+1, GCY,   x2+1, y2,   col);
  gfx->drawLine(GCX,   GCY+1, x2,   y2+1, col);
}

void drawGaugeBase(float gauge_max, const char* unit) {
  // Hintergrund
  gfx->fillScreen(COL_BG);

  // Farbzonen-Bogen
  for (float a = 210.0f; a >= -30.0f; a -= 0.5f) {
    float frac = (210.0f - a) / 240.0f;
    uint16_t col = (frac < 0.5f)  ? COL_GREEN  :
                   (frac < 0.75f) ? COL_YELLOW :
                   (frac < 0.9f)  ? COL_ORANGE : COL_RED;
    float rad = a * M_PI / 180.0f;
    for (int dr = 0; dr < 5; dr++) {
      int x = GCX + (int)((GR-dr) * cos(rad));
      int y = GCY - (int)((GR-dr) * sin(rad));
      gfx->drawPixel(x, y, col);
    }
  }

  // Skalenstriche + Beschriftung
  for (int i = 0; i <= 10; i++) {
    float frac  = i / 10.0f;
    float angle = gaugeAngle(frac);
    float rad   = angle * M_PI / 180.0f;

    // Hauptstrich
    int x1 = GCX + (int)((GR-5) * cos(rad));
    int y1 = GCY - (int)((GR-5) * sin(rad));
    int x2 = GCX + (int)((GR-22) * cos(rad));
    int y2 = GCY - (int)((GR-22) * sin(rad));
    gfx->drawLine(x1,y1,x2,y2,COL_WHITE);
    gfx->drawLine(x1+1,y1,x2+1,y2,COL_WHITE);

    // Nebenstriche zwischen Hauptstrichen
    if (i < 10) {
      for (int j = 1; j < 4; j++) {
        float sf   = (i + j*0.25f) / 10.0f;
        float sang = gaugeAngle(sf);
        float srad = sang * M_PI / 180.0f;
        int sx1 = GCX + (int)((GR-5) * cos(srad));
        int sy1 = GCY - (int)((GR-5) * sin(srad));
        int sx2 = GCX + (int)((GR-12) * cos(srad));
        int sy2 = GCY - (int)((GR-12) * sin(srad));
        gfx->drawLine(sx1,sy1,sx2,sy2,COL_DGRAY);
      }
    }

    // Beschriftung
    float val = gauge_max * frac;
    char label[8];
    if (gauge_max >= 100.0f) snprintf(label,8,"%.0f",val);
    else                      snprintf(label,8,"%.1f",val);

    int lx = GCX + (int)(GR_TXT * cos(rad)) - 8;
    int ly = GCY - (int)(GR_TXT * sin(rad)) - 5;
    gfx->setTextSize(1);
    gfx->setTextColor(COL_LGRAY);
    gfx->setCursor(lx, ly);
    gfx->print(label);
  }

  // Einheit in der Mitte
  gfx->setTextSize(2);
  gfx->setTextColor(COL_WHITE);
  int uw = strlen(unit) * 12;
  gfx->setCursor(GCX - uw/2, GCY - 45);
  gfx->print(unit);

  // Zentrumspunkt
  gfx->fillCircle(GCX, GCY, 10, COL_WHITE);
  gfx->fillCircle(GCX, GCY,  5, COL_DGRAY);
}

// ----------------------------------------------------------------
// Gesamt-Display-Update
// ----------------------------------------------------------------
float        lastFraction  = -1.0f;
float        lastGaugeMax  = -1.0f;
const char*  lastUnit      = "";

void updateDisplay(float dose_usvh, int active_count, const Source* best) {
  RangeResult rr = autoRange(dose_usvh);
  float fraction = rr.display_val / rr.gauge_max;
  fraction = constrain(fraction, 0.0f, 1.0f);

  // Skala neu zeichnen wenn Range gewechselt
  bool range_changed = (strcmp(rr.unit, lastUnit) != 0) ||
                       (rr.gauge_max != lastGaugeMax);
  if (range_changed) {
    drawGaugeBase(rr.gauge_max, rr.unit);
    lastUnit      = rr.unit;
    lastGaugeMax  = rr.gauge_max;
    lastFraction  = -1.0f;
  }

  // Nadel aktualisieren (nur wenn Änderung > 0.3%)
  if (fabs(fraction - lastFraction) > 0.003f) {
    if (lastFraction >= 0.0f) drawNeedle(lastFraction, COL_BG);
    drawNeedle(fraction, rr.color);
    gfx->fillCircle(GCX, GCY, 10, COL_WHITE);
    gfx->fillCircle(GCX, GCY,  5, COL_DGRAY);
    lastFraction = fraction;
  }

  // ---- Digitale Anzeige (oben links) ----
  gfx->fillRect(0, 0, 200, 75, COL_BG);

  gfx->setTextSize(4);
  gfx->setTextColor(rr.color);
  gfx->setCursor(5, 5);
  if      (rr.display_val < 10.0f)   gfx->printf("%.2f", rr.display_val);
  else if (rr.display_val < 100.0f)  gfx->printf("%.1f", rr.display_val);
  else                                gfx->printf("%.0f", rr.display_val);

  gfx->setTextSize(2);
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(5, 52);
  gfx->print(rr.unit);

  // ---- Info oben rechts ----
  gfx->fillRect(350, 0, 186, 75, COL_BG);
  gfx->setTextSize(1);
  gfx->setTextColor(COL_LGRAY);
  gfx->setCursor(355, 5);
  gfx->printf("Aktive Quellen: %d", active_count);

  if (best != nullptr) {
    gfx->setCursor(355, 20);
    gfx->printf("Naechste:");
    gfx->setCursor(355, 33);
    gfx->setTextColor(COL_CYAN);
    gfx->print(best->name);
    gfx->setTextColor(COL_LGRAY);
    gfx->setCursor(355, 46);
    gfx->printf("%s", nuklid_name[best->nuklid]);
    gfx->setCursor(355, 59);
    gfx->printf("RSSI: %.0f dBm", best->rssi_smooth);
  } else {
    gfx->setCursor(355, 30);
    gfx->setTextColor(COL_DGRAY);
    gfx->print("Kein Signal");
  }

  // ---- Debug-Bereich (unten) ----
  if (DEBUG_MODE) {
    gfx->fillRect(0, 175, 536, 65, COL_BG);
    gfx->drawLine(0, 176, 536, 176, COL_DGRAY);
    gfx->setTextSize(1);
    int x = 5, y = 180;
    int shown = 0;
    for (int i = 0; i < MAX_SOURCES && shown < 4; i++) {
      if (!sources[i].valid) continue;
      if ((millis()-sources[i].last_seen) > SOURCE_TIMEOUT_MS) continue;
      bool is_best = (best != nullptr && best->sender_nr == sources[i].sender_nr);
      gfx->setTextColor(is_best ? COL_CYAN : COL_LGRAY);
      gfx->setCursor(x + shown*130, y);
      gfx->printf("%s", sources[i].name);
      gfx->setCursor(x + shown*130, y+12);
      gfx->printf("%.0fdBm %.2fuSv",
        sources[i].rssi_smooth, calcDose(sources[i]));
      shown++;
    }
  }
}

// ----------------------------------------------------------------
// BLE + Setup + Loop
// ----------------------------------------------------------------
BLEScan* bleScan;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== CBRN-RN Sonde v1.0 | Debug=%s ===\n",
    DEBUG_MODE?"AN":"AUS");

  gfx->begin();
  gfx->fillScreen(COL_BG);

  // Startbildschirm
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(100, 70);
  gfx->print("CBRN-RN");
  gfx->setTextSize(2);
  gfx->setCursor(150, 120);
  gfx->print("SONDE v1.0");
  if (DEBUG_MODE) {
    gfx->setTextSize(1);
    gfx->setTextColor(COL_YELLOW);
    gfx->setCursor(180, 160);
    gfx->print("DEBUG MODE AN");
  }
  delay(1500);

  memset(sources, 0, sizeof(sources));

  BLEDevice::init("Sonde");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new ScanCallbacks(), true);
  bleScan->setActiveScan(false);
  bleScan->setInterval(100);
  bleScan->setWindow(99);
  bleScan->start(0, nullptr, false);
  Serial.println("[BLE] Scan laeuft");

  drawGaugeBase(1000.0f, "nSv/h");
  lastUnit     = "nSv/h";
  lastGaugeMax = 1000.0f;
}

void loop() {
  static uint32_t lastUpdate  = 0;
  static uint32_t lastCleanup = 0;
  uint32_t now = millis();

  // Timeout-Cleanup
  if (now - lastCleanup > 2000) {
    lastCleanup = now;
    for (int i = 0; i < MAX_SOURCES; i++) {
      if (sources[i].valid && (now-sources[i].last_seen) > SOURCE_TIMEOUT_MS) {
        Serial.printf("[BLE] Quelle %s Timeout\n", sources[i].name);
        sources[i].valid = false;
      }
    }
  }

  // Display 5Hz
  if (now - lastUpdate > 200) {
    lastUpdate = now;

    // Beste Quelle (höchster RSSI)
    const Source* best = nullptr;
    float best_rssi = -999.0f;
    int   active    = 0;

    for (int i = 0; i < MAX_SOURCES; i++) {
      if (!sources[i].valid) continue;
      if ((now-sources[i].last_seen) > SOURCE_TIMEOUT_MS) continue;
      active++;
      if (sources[i].rssi_smooth > best_rssi) {
        best_rssi = sources[i].rssi_smooth;
        best = &sources[i];
      }
    }

    float dose = (best != nullptr) ? calcDose(*best) : 0.0f;
    updateDisplay(dose, active, best);

    if (DEBUG_MODE && best != nullptr) {
      Serial.printf("[SONDE] %s | RSSI=%.0f | Peak=%.0f | %.3f uSv/h | n=%d\n",
        best->name, best->rssi_smooth, best->rssi_peak, dose, active);
    }
  }
}