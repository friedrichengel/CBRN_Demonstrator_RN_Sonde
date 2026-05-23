// ================================================================
// battery.h - LiPo-Spannung & Lade-Heuristik (TP4056 ohne CHRG-Pin)
// ================================================================
#pragma once

struct BatState {
  float voltage;
  int   percent;
  bool  charging;
};

extern BatState bat;

void initBattery();
void updateBattery();
