// ================================================================
// display.h - Canvas-Setup + Screens (Gauge, Menue, Sources, Owner)
// ================================================================
#pragma once
#include <Arduino_GFX_Library.h>
#include "ble.h"      // Source pointer in Gauge-Screen
#include "colors.h"

extern Arduino_GFX*    output;     // Hardware-Treiber (RM67162)
extern Arduino_Canvas* gfx;        // Off-Screen-Canvas (PSRAM)

void initDisplay();

void drawTopBar();
void drawVersion();              // X.YY in der Ecke unten rechts

void drawScreenGauge(float dose_usvh, int active_count, int calibrating_count, const Source* best);
void drawScreenMenu();
void drawScreenSources();
void drawScreenOwner();
