// ================================================================
// CBRN-RN Trainingsgeraet - Sonden-Firmware
// Hardware: LilyGO T-Display-S3 AMOLED (ESP32-S3, RM67162, 536x240)
//
// main.cpp ist nur Orchestrator. Module:
//   config.h    - Pinout, Tunings, Version
//   state.h     - globale Screen-/Modus-States
//   colors.h    - RGB565 Farben
//   ble.cpp     - NimBLE-Scan + Source-Datenmodell
//   dose.cpp    - Dosis-Berechnung + Auto-Range
//   audio.cpp   - Geiger-Klicks (PAM8403 via LEDC)
//   battery.cpp - LiPo-Spannung + USB-Erkennung
//   buttons.cpp - 3-Tasten mit Debounce + Short/Long
//   display.cpp - Canvas + alle Screens
//   power.cpp   - Deep Sleep
//   input.cpp   - Screen-abh. Tasten-Routing
//   owner.cpp   - Besitzer-Info-Daten
//
// Bedienung siehe README.md
// ================================================================

#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "colors.h"
#include "ble.h"
#include "dose.h"
#include "audio.h"
#include "battery.h"
#include "buttons.h"
#include "display.h"
#include "power.h"
#include "input.h"
#include "webportal.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== CBRN-RN Sonde v%s | Debug=%s ===\n",
                FIRMWARE_VERSION_STR, DEBUG_MODE ? "AN" : "AUS");

  initButtons();
  initDisplay();
  initAudio();
  initBattery();
  updateBattery();
  initBle();

  delay(800);
}

void loop() {
  uint32_t now = millis();

  handleButtons();
  if (offRequested) { enterDeepSleep(); return; }

  updateBattery();

  // WLAN-/Update-Modus: BLE ist gestoppt, daher kein Quellen-Handling.
  // Stattdessen Webserver/DNS bedienen und den WLAN-Screen zeichnen.
  if (currentScreen == SCR_WIFI) {
    handleWifiPortal();
    static uint32_t lastDisplayW = 0;
    if (now - lastDisplayW >= DISPLAY_FPS_MS) {
      lastDisplayW = now;
      gfx->fillScreen(COL_BG);
      drawScreenWifi();
      drawVersion();
      gfx->flush();
    }
    delay(5);
    return;
  }

  // Timeout-Cleanup
  static uint32_t lastCleanup = 0;
  if (now - lastCleanup > 2000) {
    lastCleanup = now;
    for (int i = 0; i < MAX_SOURCES; i++) {
      if (sources[i].valid && (now - sources[i].last_seen) > SOURCE_TIMEOUT_MS) {
        sources[i].valid = false;
      }
    }
  }

  // Beste Quelle = max(current_dose) ueber alle live, kalibrierte Quellen.
  // Quellen in der Aufwaerm-/Kalibrierphase (Ringbuffer noch nicht voll) sind
  // detektiert aber liefern noch keinen sinnvollen Dosis-Wert.
  const Source* best = nullptr;
  float best_dose   = -1.0f;
  int   active      = 0;
  int   calibrating = 0;
  for (int i = 0; i < MAX_SOURCES; i++) {
    if (!sources[i].valid) continue;
    if ((now - sources[i].last_seen) > SOURCE_TIMEOUT_MS) continue;
    active++;
    if (isCalibrating(sources[i])) { calibrating++; continue; }
    float d = calcDose(sources[i]);
    if (d > best_dose) {
      best_dose = d;
      best = &sources[i];
    }
  }
  float dose = (best != nullptr) ? best_dose : 0.0f;

  if (currentScreen == SCR_GAUGE) scheduleClicks(dose);

  // Display refresh (10 Hz)
  static uint32_t lastDisplay = 0;
  if (now - lastDisplay >= DISPLAY_FPS_MS) {
    lastDisplay = now;
    gfx->fillScreen(COL_BG);
    switch (currentScreen) {
      case SCR_GAUGE:   drawScreenGauge(dose, active, calibrating, best); break;
      case SCR_MENU:    drawScreenMenu();                    break;
      case SCR_OWNER:   drawScreenOwner();                   break;
      case SCR_SOURCES: drawScreenSources();                 break;
      case SCR_PIN:     drawScreenPin();                     break;
    }
    drawVersion();
    gfx->flush();
  }

  delay(5);
}
