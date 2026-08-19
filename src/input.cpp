// ================================================================
// input.cpp - Tasten-Routing pro Screen
// ================================================================
#include <Arduino.h>
#include "input.h"
#include "buttons.h"
#include "state.h"
#include "config.h"
#include "ble.h"
#include "audio.h"
#include "webportal.h"
#include "display.h"

// Quellen-Liste = Diagnose: ohne Freischaltung erst PIN-Eingabe oeffnen
static void openSourcesOrPin() {
  if (diagEnabled) {
    currentScreen = SCR_SOURCES;
    sourceBrowseIdx = 0;
  } else {
    for (int i = 0; i < DIAG_PIN_LEN; i++) pinDigits[i] = 0;
    pinPos = 0;
    currentScreen = SCR_PIN;
  }
}

static bool pinCorrect() {
  const char* pin = DIAG_PIN;
  for (int i = 0; i < DIAG_PIN_LEN; i++)
    if (pinDigits[i] != (uint8_t)(pin[i] - '0')) return false;
  return true;
}

static void handleMenuSelect() {
  menuLastInputMs = millis();
  playClick();
  switch (menuIndex) {
    case 0: switchBleMode(currentBleMode == BLE_NORMAL ? BLE_LR : BLE_NORMAL); break;
    case 1: setAudioEnabled(!audioEnabled);                                     break;
    case 2: openSourcesOrPin();                                                 break;
    case 3: currentScreen = SCR_OWNER;                                          break;
    case 4: initWifiPortal(); currentScreen = SCR_WIFI;                         break;
    case 5: setDisplayFlipped(!displayFlipped);                                 break;
    case 6: offRequested = true;                                                break;
  }
}

void handleButtons() {
  BtnEvent eHW = readBtn(btnHW);
  BtnEvent eA  = readBtn(btnA);
  BtnEvent eB  = readBtn(btnB);

  if (currentScreen == SCR_GAUGE) {
    if      (eHW == BTN_SHORT) { currentScreen = SCR_MENU; menuIndex = 0; menuLastInputMs = millis(); }
    else if (eHW == BTN_LONG)  { offRequested = true; }
    else if (eA  == BTN_SHORT) { setAudioEnabled(!audioEnabled); }
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
  else if (currentScreen == SCR_SOURCES) {
    if      (eHW == BTN_LONG)  { currentScreen = SCR_MENU; }
    else if (eHW == BTN_SHORT) { currentScreen = SCR_MENU; }
    else if (eA  == BTN_SHORT) { sourceBrowseIdx++; }
    else if (eB  == BTN_SHORT) { sourceBrowseIdx--; }
  }
  else if (currentScreen == SCR_PIN) {
    if      (eA  == BTN_SHORT) { pinDigits[pinPos] = (pinDigits[pinPos] + 1) % 10; playClick(); }
    else if (eB  == BTN_SHORT) { pinPos = (pinPos + 1) % DIAG_PIN_LEN;             playClick(); }
    else if (eHW == BTN_LONG)  { currentScreen = SCR_MENU; menuLastInputMs = millis(); }
    else if (eHW == BTN_SHORT) {
      playClick();
      if (pinCorrect()) {
        diagEnabled = true;   // bis zum Neustart (bewusst nicht im NVS)
        Serial.println("[UI] Diagnose-Modus freigeschaltet");
        currentScreen = SCR_SOURCES;
        sourceBrowseIdx = 0;
      } else {
        Serial.println("[UI] Diagnose-PIN falsch");
        currentScreen = SCR_MENU;
      }
      menuLastInputMs = millis();
    }
  }
  else if (currentScreen == SCR_WIFI) {
    // WLAN-Modus nur per langem HW-Druck verlassen (kurze Drucke ignorieren,
    // damit man nicht versehentlich ein laufendes Update abbricht).
    if (eHW == BTN_LONG) { stopWifiPortal(); currentScreen = SCR_MENU; }
  }
}
