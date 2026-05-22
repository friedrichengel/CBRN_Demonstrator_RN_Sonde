#include "state.h"

Screen   currentScreen   = SCR_GAUGE;
BleMode  currentBleMode  = BLE_NORMAL;
bool     audioEnabled    = true;
bool     offRequested    = false;
int      menuIndex       = 0;
uint32_t menuLastInputMs = 0;
int      sourceBrowseIdx = 0;
