#include "state.h"
#include "config.h"

Screen   currentScreen   = SCR_GAUGE;
BleMode  currentBleMode  = BLE_NORMAL;
bool     audioEnabled    = true;
bool     displayFlipped   = false;
bool     offRequested    = false;
int      menuIndex       = 0;
uint32_t menuLastInputMs = 0;
int      sourceBrowseIdx = 0;

bool     diagEnabled     = DIAG_MODE_DEFAULT;
uint8_t  pinDigits[DIAG_PIN_LEN] = {0};
int      pinPos          = 0;
