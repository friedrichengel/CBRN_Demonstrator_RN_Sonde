// ================================================================
// buttons.h - 3-Tasten-Eingang mit Debounce + Short/Long-Press
// ================================================================
#pragma once
#include <stdint.h>

struct Btn {
  uint8_t  pin;
  bool     stable;
  bool     lastRaw;
  uint32_t lastChangeMs;
  uint32_t pressStartMs;
  bool     longFired;
};

enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_LONG };

extern Btn btnHW;
extern Btn btnA;
extern Btn btnB;

void     initButtons();
BtnEvent readBtn(Btn& b);
