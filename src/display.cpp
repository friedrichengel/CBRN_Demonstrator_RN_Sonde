// ================================================================
// display.cpp - Canvas, Gauge, Top-Bar, Menue, Sources, Owner-Screen
// ================================================================
#include <Arduino.h>
#include <math.h>
#include "display.h"
#include "config.h"
#include "state.h"
#include "ble.h"
#include "dose.h"
#include "battery.h"
#include "owner.h"

// ---- Hardware-Treiber + Canvas (in PSRAM) ----
static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  PIN_QSPI_CS, PIN_QSPI_SCK, PIN_QSPI_D0, PIN_QSPI_D1, PIN_QSPI_D2, PIN_QSPI_D3);
Arduino_GFX    *output = new Arduino_RM67162(bus, PIN_TFT_RST, 1);
// 536*240*2 = 257 KB Canvas
Arduino_Canvas *gfx    = new Arduino_Canvas(536, 240, output);

// ---- Gauge-Geometrie ----
static const int GCX    = 268;
static const int GCY    = 205;
static const int GR     = 135;
static const int GR_TXT = 112;
static const int GR_NDL = 120;

static float gaugeAngle(float fraction) {
  return 195.0f - constrain(fraction, 0.0f, 1.0f) * 210.0f;
}

static void drawNeedle(float fraction, uint16_t col) {
  float rad = gaugeAngle(fraction) * M_PI / 180.0f;
  int x2 = GCX + (int)(GR_NDL * cos(rad));
  int y2 = GCY - (int)(GR_NDL * sin(rad));
  gfx->drawLine(GCX,   GCY,   x2,   y2,   col);
  gfx->drawLine(GCX+1, GCY,   x2+1, y2,   col);
  gfx->drawLine(GCX,   GCY+1, x2,   y2+1, col);
}

static void drawGaugeBase(float gauge_max, const char* unit) {
  for (float a = 195.0f; a >= -15.0f; a -= 0.5f) {
    float frac = (195.0f - a) / 210.0f;
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
  for (int i = 0; i <= 10; i++) {
    float frac = i / 10.0f;
    float rad  = gaugeAngle(frac) * M_PI / 180.0f;
    int x1 = GCX + (int)((GR-5)  * cos(rad));
    int y1 = GCY - (int)((GR-5)  * sin(rad));
    int x2 = GCX + (int)((GR-22) * cos(rad));
    int y2 = GCY - (int)((GR-22) * sin(rad));
    gfx->drawLine(x1,y1,x2,y2,COL_WHITE);
    gfx->drawLine(x1+1,y1,x2+1,y2,COL_WHITE);
    if (i < 10) {
      for (int j = 1; j < 4; j++) {
        float srad = gaugeAngle((i + j*0.25f) / 10.0f) * M_PI / 180.0f;
        int sx1 = GCX + (int)((GR-5)  * cos(srad));
        int sy1 = GCY - (int)((GR-5)  * sin(srad));
        int sx2 = GCX + (int)((GR-12) * cos(srad));
        int sy2 = GCY - (int)((GR-12) * sin(srad));
        gfx->drawLine(sx1,sy1,sx2,sy2,COL_DGRAY);
      }
    }
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
  gfx->setTextSize(2);
  gfx->setTextColor(COL_WHITE);
  int uw = strlen(unit) * 12;
  gfx->setCursor(GCX - uw/2, GCY - 45);
  gfx->print(unit);
  gfx->fillCircle(GCX, GCY, 10, COL_WHITE);
  gfx->fillCircle(GCX, GCY,  5, COL_DGRAY);
}

// ====================== TOP-BAR ==================================
void drawTopBar() {
  gfx->setTextSize(1);

  int bx = 5, by = 5, bw = 36, bh = 14;
  gfx->drawRect(bx, by, bw, bh, COL_WHITE);
  gfx->fillRect(bx + bw, by + 3, 3, bh - 6, COL_WHITE);
  int fill_w = (bw - 2) * bat.percent / 100;
  uint16_t bcol = (bat.percent < 20) ? COL_RED :
                  (bat.percent < 50) ? COL_YELLOW : COL_GREEN;
  if (fill_w > 0) gfx->fillRect(bx + 1, by + 1, fill_w, bh - 2, bcol);
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(bx + bw + 8, by + 4);
  gfx->printf("%d%%", bat.percent);
  if (bat.charging) {
    gfx->setTextColor(COL_CYAN);
    gfx->setCursor(bx + bw + 42, by + 4);
    gfx->print("USB");
  }

  gfx->setCursor(465, 9);
  gfx->setTextColor(audioEnabled ? COL_LGRAY : COL_DGRAY);
  gfx->print("AUD");

  gfx->setCursor(495, 9);
  gfx->setTextColor(currentBleMode == BLE_LR ? COL_CYAN : COL_LGRAY);
  gfx->print(currentBleMode == BLE_LR ? "LR" : "1M");
}

// ====================== VERSION (unten rechts) ===================
void drawVersion() {
  // Format X.YY (Major.Minor) -> z.B. "2.10"
  gfx->setTextSize(1);
  gfx->setTextColor(COL_DGRAY);
  gfx->setCursor(508, 230);
  gfx->print(FIRMWARE_VERSION_STR);
}

// ====================== SCREEN: GAUGE ============================
void drawScreenGauge(float dose_usvh, int active_count, int calibrating_count,
                     const Source* best) {
  float max_d = (best != nullptr) ? best->max_dose_usvh : 1.0f;
  RangeResult rr = autoRange(dose_usvh, max_d);
  float fraction = constrain(rr.display_val / rr.gauge_max, 0.0f, 1.0f);

  drawGaugeBase(rr.gauge_max, rr.unit);
  drawNeedle(fraction, rr.color);
  gfx->fillCircle(GCX, GCY, 10, COL_WHITE);
  gfx->fillCircle(GCX, GCY,  5, COL_DGRAY);

  // Linke Spalte: grosser Wert. Waehrend Kalibrierung "----" statt 0-Wert.
  gfx->setTextSize(4);
  gfx->setCursor(5, 28);
  if (best == nullptr && calibrating_count > 0) {
    gfx->setTextColor(COL_YELLOW);
    gfx->print("----");
  } else {
    gfx->setTextColor(rr.color);
    if      (rr.display_val < 10.0f)  gfx->printf("%.2f", rr.display_val);
    else if (rr.display_val < 100.0f) gfx->printf("%.1f", rr.display_val);
    else                               gfx->printf("%.0f", rr.display_val);
  }
  gfx->setTextSize(2);
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(5, 78);
  gfx->print(rr.unit);

  gfx->setTextSize(1);
  gfx->setTextColor(COL_LGRAY);
  gfx->setCursor(380, 30);
  gfx->printf("Quellen: %d", active_count);
  if (best != nullptr) {
    gfx->setCursor(380, 45);
    gfx->print("Naechste:");
    gfx->setTextColor(COL_CYAN);
    gfx->setCursor(380, 58);
    gfx->print(best->name);
    gfx->setTextColor(COL_LGRAY);
    gfx->setCursor(380, 71);
    gfx->print(nuklid_name[best->nuklid]);
    gfx->setCursor(380, 84);
    gfx->printf("RSSI %.0fdBm", best->rssi_smooth);
  } else if (calibrating_count > 0) {
    gfx->setCursor(380, 55);
    gfx->setTextColor(COL_YELLOW);
    gfx->print("Kalibrierung...");
  } else {
    gfx->setCursor(380, 55);
    gfx->setTextColor(COL_DGRAY);
    gfx->print("Kein Signal");
  }

  drawTopBar();
}

// ====================== SCREEN: MENU =============================
void drawScreenMenu() {
  gfx->setTextSize(2);
  gfx->setTextColor(COL_CYAN);
  gfx->setCursor(15, 28);
  gfx->print("Menue");

  const char* rows[MENU_ITEMS];
  char buf0[40], buf1[40];
  snprintf(buf0, sizeof(buf0), "Reichweite: %s",
           currentBleMode == BLE_LR ? "Lang (LR)" : "Normal");
  snprintf(buf1, sizeof(buf1), "Audio:      %s",
           audioEnabled ? "AN" : "AUS");
  rows[0] = buf0;
  rows[1] = buf1;
  rows[2] = "Quellen-Liste";
  rows[3] = "Besitzer-Info";
  rows[4] = "Aus-Modus";

  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = 60 + i * 30;
    if (i == menuIndex) {
      gfx->fillRect(10, y - 5, 516, 26, COL_SEL);
      gfx->setTextColor(COL_WHITE);
    } else {
      gfx->setTextColor(COL_LGRAY);
    }
    gfx->setTextSize(2);
    gfx->setCursor(20, y);
    gfx->print(rows[i]);
  }

  drawTopBar();
}

// ====================== SCREEN: SOURCES ==========================
void drawScreenSources() {
  int idxs[MAX_SOURCES];
  int count = 0;
  uint32_t now = millis();
  for (int i = 0; i < MAX_SOURCES; i++) {
    if (!sources[i].valid) continue;
    if ((now - sources[i].last_seen) > SOURCE_TIMEOUT_MS) continue;
    idxs[count++] = i;
  }

  gfx->setTextSize(2);
  gfx->setTextColor(COL_CYAN);
  gfx->setCursor(15, 28);

  if (count == 0) {
    gfx->print("Quellen");
    gfx->setTextSize(2);
    gfx->setTextColor(COL_DGRAY);
    gfx->setCursor(110, 110);
    gfx->print("Keine Quelle empfangen");
    gfx->setTextSize(1);
    gfx->setTextColor(COL_DGRAY);
    gfx->setCursor(15, 222);
    gfx->print("HW lang: zurueck");
    drawTopBar();
    return;
  }

  if (sourceBrowseIdx >= count) sourceBrowseIdx = 0;
  if (sourceBrowseIdx < 0)      sourceBrowseIdx = count - 1;
  const Source& s = sources[idxs[sourceBrowseIdx]];

  gfx->printf("Quelle %d / %d", sourceBrowseIdx + 1, count);

  gfx->setTextSize(2);
  gfx->setTextColor(COL_LGRAY);
  gfx->setCursor(15, 62);
  gfx->print("<");
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(60, 62);
  gfx->print(s.name);
  gfx->setTextColor(COL_LGRAY);
  gfx->setCursor(510, 62);
  gfx->print(">");

  gfx->setTextSize(1);
  gfx->setTextColor(COL_LGRAY);
  gfx->setCursor(15, 95);
  gfx->printf("Nuklid: %s   Sender-Nr: %03d   TX: %+d dBm",
              nuklid_name[s.nuklid], s.sender_nr, s.tx_power);

  float maxd = s.max_dose_usvh;
  RangeResult rrMax = autoRange(maxd, maxd);

  auto printDose = [&](int y, const char* lbl, const RangeResult& r, uint16_t col) {
    gfx->setTextSize(2);
    gfx->setTextColor(col);
    gfx->setCursor(15, y);
    if      (r.display_val < 10.0f)   gfx->printf("%s %.2f %s", lbl, r.display_val, r.unit);
    else if (r.display_val < 100.0f)  gfx->printf("%s %.1f %s", lbl, r.display_val, r.unit);
    else                               gfx->printf("%s %.0f %s", lbl, r.display_val, r.unit);
  };

  if (isCalibrating(s)) {
    gfx->setTextSize(2);
    gfx->setTextColor(COL_YELLOW);
    gfx->setCursor(15, 120);
    gfx->print("Aktuell: Kalibrierung...");
  } else {
    float cur = calcDose(s);
    RangeResult rrCur = autoRange(cur, maxd);
    printDose(120, "Aktuell:", rrCur, rrCur.color);
  }
  printDose(148, "Max:    ", rrMax, COL_LGRAY);

  gfx->setTextSize(1);
  gfx->setTextColor(COL_LGRAY);
  gfx->setCursor(15, 180);
  gfx->printf("RSSI: %.0f dBm   Peak: %.0f dBm",
              s.rssi_smooth, s.rssi_peak);

  gfx->setTextSize(1);
  gfx->setTextColor(COL_DGRAY);
  gfx->setCursor(15, 222);
  gfx->print("A: vor   B: zurueck   HW: zurueck zum Menue");

  drawTopBar();
}

// ====================== SCREEN: OWNER ============================
void drawScreenOwner() {
  // Titel + Subtitel (Header)
  gfx->setTextSize(3);
  gfx->setTextColor(COL_CYAN);
  gfx->setCursor(15, 28);
  gfx->print(OWNER.title);

  gfx->setTextSize(2);
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(15, 62);
  gfx->print(OWNER.subtitle);

  // Trennlinie
  gfx->drawLine(15, 88, 521, 88, COL_DGRAY);

  // Adressblock
  gfx->setTextSize(2);
  gfx->setTextColor(COL_LGRAY);
  gfx->setCursor(15, 100);
  gfx->print(OWNER.addr_line1);
  gfx->setCursor(15, 124);
  gfx->print(OWNER.addr_line2);

  // Telefon / Fax (zweispaltig auf einer Hoehe)
  gfx->setCursor(15, 156);
  gfx->print(OWNER.phone);
  gfx->setCursor(280, 156);
  gfx->print(OWNER.fax);

  // Email (kleiner damit lange Adressen passen)
  gfx->setTextSize(1);
  gfx->setTextColor(COL_CYAN);
  gfx->setCursor(15, 188);
  gfx->print(OWNER.email);

  // Footer
  gfx->setTextSize(1);
  gfx->setTextColor(COL_DGRAY);
  gfx->setCursor(15, 222);
  gfx->print("Beliebige Taste -> zurueck");

  drawTopBar();
}

// ====================== INIT =====================================
void initDisplay() {
  gfx->begin();
  gfx->fillScreen(COL_BG);
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(150, 80);
  gfx->print("CBRN-RN");
  gfx->setTextSize(2);
  gfx->setCursor(170, 130);
  gfx->printf("SONDE v%s", FIRMWARE_VERSION_STR);
  gfx->flush();
}
