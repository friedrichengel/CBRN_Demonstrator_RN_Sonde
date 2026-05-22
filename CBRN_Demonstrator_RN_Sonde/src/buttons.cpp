// ================================================================
// buttons.cpp - Debounce, Short-/Long-Press-Detektion
// ================================================================
#include <Arduino.h>
#include "buttons.h"
#include "config.h"

Btn btnHW = { PIN_BTN_HW, true, true, 0, 0, false };
Btn btnA  = { PIN_BTN_A,  true, true, 0, 0, false };
Btn btnB  = { PIN_BTN_B,  true, true, 0, 0, false };

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
