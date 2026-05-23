// ================================================================
// state.h - Cross-cutting Enums + globale UI-/App-State Variablen
// ================================================================
#pragma once
#include <stdint.h>

enum Screen  { SCR_GAUGE, SCR_MENU, SCR_OWNER, SCR_SOURCES };
enum BleMode { BLE_NORMAL = 0, BLE_LR = 1 };

extern Screen   currentScreen;
extern BleMode  currentBleMode;
extern bool     audioEnabled;
extern bool     offRequested;
extern int      menuIndex;
extern uint32_t menuLastInputMs;
extern int      sourceBrowseIdx;
