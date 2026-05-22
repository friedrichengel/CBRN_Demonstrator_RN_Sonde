// ================================================================
// CBRN-RN Trainingsgeraet - Sonden-Firmware v2.0
// Hardware: LilyGO T-Display-S3 AMOLED (ESP32-S3, RM67162, 536x240)
//
// Funktionsumfang:
//   - NimBLE-Arduino Stack (Scan auf 1M-PHY oder Coded-PHY/LR)
//   - 3 Tasten:  HW=GPIO0  /  A=GPIO12  /  B=GPIO13
//   - Audio:     PAM8403 an GPIO14 (LEDC-PWM, Geiger-Klick)
//   - Battery:   ADC GPIO4 (BAT_VOLT, Spannungsteiler 1:2)
//   - Screens:   Gauge | Menue | Besitzer-Info
//   - Deep Sleep (Aus-Modus)
//
// Bedienung:
//   GAUGE:  HW kurz -> Menue ; HW lang -> Aus-Modus
//           A  kurz -> Audio toggle
//           B  kurz -> Reichweite toggle (Normal/LR)
//   MENUE:  HW kurz -> Auswahl bestaetigen ; HW lang -> zurueck
//           A  kurz -> hoch  ; B kurz -> runter
//   OWNER:  beliebige Taste -> zurueck
// ================================================================

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <NimBLEDevice.h>
#include <esp_sleep.h>
#include <math.h>

// ====================== PINOUT ===================================
#define PIN_QSPI_CS    6
#define PIN_QSPI_SCK   47
#define PIN_QSPI_D0    18
#define PIN_QSPI_D1    7
#define PIN_QSPI_D2    48
#define PIN_QSPI_D3    5
#define PIN_TFT_RST    17

#define PIN_BTN_HW     0     // Onboard-Boot-Button (active low)
#define PIN_BTN_A      12    // extern, INPUT_PULLUP, active low
#define PIN_BTN_B      13    // extern, INPUT_PULLUP, active low
#define PIN_AUDIO      14    // PAM8403 IN (LEDC PWM)
#define PIN_BAT_VOLT   4     // ADC1_CH3, BAT_VOLT (Teiler 1:2)

// ====================== CONFIG ===================================
#define DEBUG_MODE          false
#define MAX_SOURCES         16
#define SOURCE_TIMEOUT_MS   5000
#define RSSI_SMOOTH         0.3f

#define BTN_DEBOUNCE_MS     30
#define BTN_LONG_MS         1500

#define DISPLAY_FPS_MS      100      // 10 Hz
#define BAT_SAMPLE_MS       2000
#define MENU_TIMEOUT_MS     20000

#define AUDIO_LEDC_FREQ     4000
#define AUDIO_LEDC_RES      8
#define AUDIO_CLICK_MS      2

// ====================== DISPLAY ==================================
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  PIN_QSPI_CS, PIN_QSPI_SCK, PIN_QSPI_D0, PIN_QSPI_D1, PIN_QSPI_D2, PIN_QSPI_D3);
Arduino_GFX *output = new Arduino_RM67162(bus, PIN_TFT_RST, 1);
// 536*240*2 = 257 KB Canvas in PSRAM
Arduino_Canvas *gfx = new Arduino_Canvas(536, 240, output);

#define COL_BG     0x0000
#define COL_GREEN  0x07E0
#define COL_YELLOW 0xFFE0
#define COL_ORANGE 0xFD20
#define COL_RED    0xF800
#define COL_WHITE  0xFFFF
#define COL_LGRAY  0xC618
#define COL_DGRAY  0x4208
#define COL_CYAN   0x07FF
#define COL_SEL    0x2104   // dezent dunkelblau fuer Menue-Selektion

// ====================== NUKLIDS / SOURCES ========================
const char* nuklid_name[9] = {
  "", "Cs-137", "Co-60", "Am-241", "Ir-192",
  "Ra-226", "Th-232", "U-238", "Pu-239"
};

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

enum Screen    { SCR_GAUGE, SCR_MENU, SCR_OWNER };
enum BleMode   { BLE_NORMAL = 0, BLE_LR = 1 };
enum RangeBand { R_NSV = 0, R_USV = 1, R_MSV = 2 };

struct BatState { float voltage; int percent; bool charging; };

struct Btn {
  uint8_t  pin;
  bool     stable;
  bool     lastRaw;
  uint32_t lastChangeMs;
  uint32_t pressStartMs;
  bool     longFired;
};
enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_LONG };

struct RangeResult {
  float       display_val;
  float       gauge_max;
  const char* unit;
  uint16_t    color;
};

// ====================== STATE ====================================
Screen      currentScreen   = SCR_GAUGE;
BleMode     currentBleMode  = BLE_NORMAL;
RangeBand   currentRange    = R_NSV;
bool        audioEnabled    = true;
bool        offRequested    = false;
uint32_t    menuLastInputMs = 0;
int         menuIndex       = 0;
BatState    bat             = {3.7f, 50, false};
uint32_t    batLastSampleMs = 0;
NimBLEScan* bleScan         = nullptr;

// Besitzer-Info (Platzhalter - spaeter aus Config laden)
const char* OWNER_LINES[] = {
  "Max Mustermann",
  "Behoerde / Abteilung",
  "Inventar-Nr.: 0000",
  "Kontakt: 0000/0000000"
};
#define OWNER_LINE_COUNT (sizeof(OWNER_LINES)/sizeof(OWNER_LINES[0]))

Btn btnHW = { PIN_BTN_HW, true, true, 0, 0, false };
Btn btnA  = { PIN_BTN_A,  true, true, 0, 0, false };
Btn btnB  = { PIN_BTN_B,  true, true, 0, 0, false };

// ====================== AUDIO ====================================
void initAudio() {
  ledcAttach(PIN_AUDIO, AUDIO_LEDC_FREQ, AUDIO_LEDC_RES);
  ledcWrite(PIN_AUDIO, 0);
}

void playClick() {
  if (!audioEnabled) return;
  ledcWrite(PIN_AUDIO, 128);
  delay(AUDIO_CLICK_MS);
  ledcWrite(PIN_AUDIO, 0);
}

void scheduleClicks(float dose_usvh) {
  static uint32_t nextClickMs = 0;
  uint32_t now = millis();
  if (now < nextClickMs) return;

  // 0.3 cps Hintergrund + dose-proportional, gedeckelt
  float cps = 0.3f + dose_usvh * 0.5f;
  if (cps > 50.0f) cps = 50.0f;

  uint32_t mean_ms = (uint32_t)(1000.0f / cps);
  uint32_t jitter  = mean_ms / 2 + random(mean_ms);  // Poisson-light

  playClick();
  nextClickMs = now + jitter;
}

// ====================== BATTERY ==================================
float readBatteryVoltage() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_BAT_VOLT);
  return ((sum / 8) * 2.0f) / 1000.0f;   // Teiler 1:2
}

int batteryPercent(float v) {
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

// ====================== BUTTONS ==================================
void initButtons() {
  pinMode(PIN_BTN_HW, INPUT_PULLUP);
  pinMode(PIN_BTN_A,  INPUT_PULLUP);
  pinMode(PIN_BTN_B,  INPUT_PULLUP);
}

BtnEvent readBtn(Btn &b) {
  bool raw = digitalRead(b.pin);
  uint32_t now = millis();

  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.lastChangeMs = now;
  }

  if (raw != b.stable && (now - b.lastChangeMs) > BTN_DEBOUNCE_MS) {
    b.stable = raw;
    if (!raw) {           // gedrueckt
      b.pressStartMs = now;
      b.longFired = false;
    } else {              // losgelassen
      if (!b.longFired) return BTN_SHORT;
    }
  }

  if (!b.stable && !b.longFired && (now - b.pressStartMs) > BTN_LONG_MS) {
    b.longFired = true;
    return BTN_LONG;
  }
  return BTN_NONE;
}

// ====================== BLE (NimBLE) =============================
class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
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

    int idx = -1;
    for (int i = 0; i < MAX_SOURCES; i++)
      if (sources[i].valid && sources[i].sender_nr == sender_nr) { idx = i; break; }

    if (idx < 0) {
      for (int i = 0; i < MAX_SOURCES; i++) if (!sources[i].valid) { idx = i; break; }
      if (idx < 0) return;
      sources[idx].valid       = true;
      sources[idx].rssi_smooth = (float)rssi;
      sources[idx].rssi_peak   = (float)rssi;
      if (dev->haveName()) {
        strncpy(sources[idx].name, dev->getName().c_str(), 19);
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
    s.rssi_smooth    = s.rssi_smooth*(1.0f-RSSI_SMOOTH) + (float)rssi*RSSI_SMOOTH;
    if (s.rssi_smooth > s.rssi_peak) s.rssi_peak = s.rssi_smooth;
  }
};
ScanCB scanCallbacks;

void startScan() {
  if (!bleScan) bleScan = NimBLEDevice::getScan();
  bleScan->stop();
  bleScan->setScanCallbacks(&scanCallbacks, true);
  bleScan->setActiveScan(false);
  // Duty ~50% (Window 50 / Interval 100) -> nimble_host blockiert
  // IDLE0 nicht mehr, WDT bleibt zufrieden. Vorher 99/100 = WDT-Reset.
  bleScan->setInterval(100);
  bleScan->setWindow(50);

  // TODO: Coded PHY (BLE 5 LR) per Extended Scan aktivieren.
  // Exakte API in NimBLE-Arduino 2.x: nach erstem erfolgreichem Build pruefen.
  // Kandidaten: bleScan->setPhy(...) | bleScan->setExtScanPhy(...) |
  //             NimBLEDevice::setScanPhy(...). Bis dahin scannt der Stack
  //             immer auf 1M-PHY, der Menue-Toggle setzt nur das Flag.
  (void)currentBleMode;

  bleScan->start(0, false);
}

void switchBleMode(BleMode newMode) {
  if (newMode == currentBleMode) return;
  currentBleMode = newMode;
  startScan();
  Serial.printf("[BLE] Mode -> %s\n", newMode == BLE_LR ? "LR/Coded" : "Normal/1M");
}

// ====================== DOSE / RANGE =============================
float calcDose(const Source &s) {
  float delta_db = s.rssi_smooth - s.rssi_peak;
  float max_dose = s.max_dosis_100 / 100.0f;
  return max_dose * pow(10.0f, delta_db / 10.0f);
}

RangeResult autoRange(float dose_usvh) {
  if      (currentRange == R_NSV && dose_usvh >= 1.0f)    currentRange = R_USV;
  else if (currentRange == R_USV && dose_usvh <  0.7f)    currentRange = R_NSV;
  else if (currentRange == R_USV && dose_usvh >= 1000.0f) currentRange = R_MSV;
  else if (currentRange == R_MSV && dose_usvh <  700.0f)  currentRange = R_USV;

  if (currentRange == R_NSV) {
    float v = dose_usvh * 1000.0f;
    uint16_t c = (v < 300) ? COL_GREEN : (v < 700) ? COL_YELLOW : COL_ORANGE;
    return {v, 1000.0f, "nSv/h", c};
  } else if (currentRange == R_USV) {
    uint16_t c = (dose_usvh < 100) ? COL_YELLOW :
                 (dose_usvh < 500) ? COL_ORANGE : COL_RED;
    return {dose_usvh, 1000.0f, "uSv/h", c};
  } else {
    return {dose_usvh / 1000.0f, 10.0f, "mSv/h", COL_RED};
  }
}

// ====================== GAUGE ====================================
#define GCX     268
#define GCY     205
#define GR      135
#define GR_TXT  112
#define GR_NDL  120

float gaugeAngle(float fraction) {
  return 195.0f - constrain(fraction, 0.0f, 1.0f) * 210.0f;
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
    float frac  = i / 10.0f;
    float rad   = gaugeAngle(frac) * M_PI / 180.0f;
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

// ====================== TOP-BAR (Battery + BLE) ==================
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

// ====================== SCREEN: GAUGE ============================
void drawScreenGauge(float dose_usvh, int active_count, const Source* best) {
  RangeResult rr = autoRange(dose_usvh);
  float fraction = constrain(rr.display_val / rr.gauge_max, 0.0f, 1.0f);

  drawGaugeBase(rr.gauge_max, rr.unit);
  drawNeedle(fraction, rr.color);
  gfx->fillCircle(GCX, GCY, 10, COL_WHITE);
  gfx->fillCircle(GCX, GCY,  5, COL_DGRAY);

  gfx->setTextSize(4);
  gfx->setTextColor(rr.color);
  gfx->setCursor(5, 28);
  if      (rr.display_val < 10.0f)  gfx->printf("%.2f", rr.display_val);
  else if (rr.display_val < 100.0f) gfx->printf("%.1f", rr.display_val);
  else                               gfx->printf("%.0f", rr.display_val);
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
  } else {
    gfx->setCursor(380, 55);
    gfx->setTextColor(COL_DGRAY);
    gfx->print("Kein Signal");
  }

  drawTopBar();
}

// ====================== SCREEN: MENU =============================
#define MENU_ITEMS 4

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
  rows[2] = "Besitzer-Info";
  rows[3] = "Aus-Modus";

  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = 70 + i * 34;
    if (i == menuIndex) {
      gfx->fillRect(10, y - 6, 516, 30, COL_SEL);
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

// ====================== SCREEN: OWNER ============================
void drawScreenOwner() {
  gfx->setTextSize(2);
  gfx->setTextColor(COL_CYAN);
  gfx->setCursor(15, 28);
  gfx->print("Besitzer");

  for (int i = 0; i < (int)OWNER_LINE_COUNT; i++) {
    gfx->setTextSize(2);
    gfx->setTextColor(COL_WHITE);
    gfx->setCursor(25, 70 + i * 30);
    gfx->print(OWNER_LINES[i]);
  }

  gfx->setTextSize(1);
  gfx->setTextColor(COL_DGRAY);
  gfx->setCursor(15, 222);
  gfx->print("Beliebige Taste -> zurueck");

  drawTopBar();
}

// ====================== POWER ====================================
void enterDeepSleep() {
  Serial.println("[PWR] -> Deep Sleep");
  gfx->fillScreen(COL_BG);
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(210, 95);
  gfx->print("Aus");
  gfx->setTextSize(1);
  gfx->setTextColor(COL_LGRAY);
  gfx->setCursor(115, 150);
  gfx->print("HW-Taste druecken zum Einschalten");
  gfx->flush();
  delay(800);

  output->displayOff();
  // Wake-up auf HW-Button (active low)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_HW, 0);
  esp_deep_sleep_start();
}

// ====================== INPUT HANDLING ===========================
void handleMenuSelect() {
  menuLastInputMs = millis();
  playClick();
  switch (menuIndex) {
    case 0: switchBleMode(currentBleMode == BLE_NORMAL ? BLE_LR : BLE_NORMAL); break;
    case 1: audioEnabled = !audioEnabled;                                       break;
    case 2: currentScreen = SCR_OWNER;                                          break;
    case 3: offRequested = true;                                                break;
  }
}

void handleButtons() {
  BtnEvent eHW = readBtn(btnHW);
  BtnEvent eA  = readBtn(btnA);
  BtnEvent eB  = readBtn(btnB);

  if (currentScreen == SCR_GAUGE) {
    if      (eHW == BTN_SHORT) { currentScreen = SCR_MENU; menuIndex = 0; menuLastInputMs = millis(); }
    else if (eHW == BTN_LONG)  { offRequested = true; }
    else if (eA  == BTN_SHORT) { audioEnabled = !audioEnabled; }
    else if (eB  == BTN_SHORT) { switchBleMode(currentBleMode == BLE_NORMAL ? BLE_LR : BLE_NORMAL); }
  }
  else if (currentScreen == SCR_MENU) {
    if      (eHW == BTN_SHORT) { handleMenuSelect(); }
    else if (eHW == BTN_LONG)  { currentScreen = SCR_GAUGE; }
    else if (eA  == BTN_SHORT) { menuIndex = (menuIndex + MENU_ITEMS - 1) % MENU_ITEMS; menuLastInputMs = millis(); }
    else if (eB  == BTN_SHORT) { menuIndex = (menuIndex + 1) % MENU_ITEMS;              menuLastInputMs = millis(); }
    if (millis() - menuLastInputMs > MENU_TIMEOUT_MS) currentScreen = SCR_GAUGE;
  }
  else if (currentScreen == SCR_OWNER) {
    if (eHW == BTN_SHORT || eA == BTN_SHORT || eB == BTN_SHORT)
      currentScreen = SCR_MENU;
  }
}

// ====================== SETUP / LOOP =============================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== CBRN-RN Sonde v2.0 | Debug=%s ===\n", DEBUG_MODE ? "AN" : "AUS");

  initButtons();

  gfx->begin();
  gfx->fillScreen(COL_BG);
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(150, 80);
  gfx->print("CBRN-RN");
  gfx->setTextSize(2);
  gfx->setCursor(170, 130);
  gfx->print("SONDE v2.0");
  gfx->flush();

  initAudio();
  updateBattery();

  memset(sources, 0, sizeof(sources));
  NimBLEDevice::init("Sonde");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  startScan();
  Serial.println("[BLE] NimBLE scan running");

  delay(800);
}

void loop() {
  uint32_t now = millis();

  handleButtons();
  if (offRequested) { enterDeepSleep(); return; }

  updateBattery();

  static uint32_t lastCleanup = 0;
  if (now - lastCleanup > 2000) {
    lastCleanup = now;
    for (int i = 0; i < MAX_SOURCES; i++) {
      if (sources[i].valid && (now - sources[i].last_seen) > SOURCE_TIMEOUT_MS) {
        sources[i].valid = false;
      }
    }
  }

  // Beste Quelle ermitteln
  const Source* best = nullptr;
  float best_rssi = -999.0f;
  int   active    = 0;
  for (int i = 0; i < MAX_SOURCES; i++) {
    if (!sources[i].valid) continue;
    if ((now - sources[i].last_seen) > SOURCE_TIMEOUT_MS) continue;
    active++;
    if (sources[i].rssi_smooth > best_rssi) {
      best_rssi = sources[i].rssi_smooth;
      best = &sources[i];
    }
  }
  float dose = (best != nullptr) ? calcDose(*best) : 0.0f;

  if (currentScreen == SCR_GAUGE) scheduleClicks(dose);

  static uint32_t lastDisplay = 0;
  if (now - lastDisplay >= DISPLAY_FPS_MS) {
    lastDisplay = now;
    gfx->fillScreen(COL_BG);
    switch (currentScreen) {
      case SCR_GAUGE: drawScreenGauge(dose, active, best); break;
      case SCR_MENU:  drawScreenMenu();                    break;
      case SCR_OWNER: drawScreenOwner();                   break;
    }
    gfx->flush();
  }

  delay(5);
}
