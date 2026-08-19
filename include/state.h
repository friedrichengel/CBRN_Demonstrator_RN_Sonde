// ================================================================
// state.h - Cross-cutting Enums + globale UI-/App-State Variablen
// ================================================================
#pragma once
#include <stdint.h>

enum Screen  { SCR_GAUGE, SCR_MENU, SCR_OWNER, SCR_SOURCES, SCR_WIFI, SCR_PIN };
enum BleMode { BLE_NORMAL = 0, BLE_LR = 1 };

extern Screen   currentScreen;
extern BleMode  currentBleMode;
extern bool     audioEnabled;
extern bool     displayFlipped;   // false = Querformat normal, true = um 180 Grad gedreht
extern bool     offRequested;
extern int      menuIndex;
extern uint32_t menuLastInputMs;
extern int      sourceBrowseIdx;

// Diagnose-Modus: nur Laufzeit-Flag, kein NVS -> nach Neustart wieder Default
extern bool     diagEnabled;
extern uint8_t  pinDigits[];      // aktuelle PIN-Eingabe (DIAG_PIN_LEN Ziffern)
extern int      pinPos;           // Cursor-Position in der PIN-Eingabe
