// ================================================================
// config.h - Zentrale Build-Konstanten (Pinout, Tunings, Version)
// ================================================================
#pragma once

// ---- Firmware-Version (X.YY) ----
#define FIRMWARE_VERSION_MAJOR  2
#define FIRMWARE_VERSION_MINOR  17
#define FIRMWARE_VERSION_STR    "2.17"

// ---- Debug ----
#define DEBUG_MODE              false

// ---- Pinout: Display QSPI (LilyGO T-Display-S3 AMOLED) ----
#define PIN_QSPI_CS    6
#define PIN_QSPI_SCK   47
#define PIN_QSPI_D0    18
#define PIN_QSPI_D1    7
#define PIN_QSPI_D2    48
#define PIN_QSPI_D3    5
#define PIN_TFT_RST    17

// ---- Pinout: Bedienelemente ----
#define PIN_BTN_HW     0     // Onboard-Boot-Button (active low)
#define PIN_BTN_A      12    // extern, INPUT_PULLUP, active low
#define PIN_BTN_B      13    // extern, INPUT_PULLUP, active low
#define PIN_AUDIO      14    // PAM8403 IN (LEDC PWM)
#define PIN_BAT_VOLT   4     // ADC1_CH3, BAT_VOLT (Spannungsteiler 1:2)

// ---- USB-VBUS-Detection (Lade-Erkennung) ----
//   Der ESP32-S3 internal-PHY liefert kein zuverlaessiges VBUS-Signal,
//   und der TP4056-CHRG-Pin geht auf der T-Display-S3 nicht zum ESP.
//   Strategie:
//     - Wenn Pin definiert (>= 0): Pin wird mit INPUT_PULLDOWN initialisiert
//       und zusaetzlich zur Software-Heuristik OR-verknuepft.
//       Unverloetet: Pin ist LOW -> nur SW-Heuristik aktiv.
//       Nach Loetmod: Pin liefert sofortiges, eindeutiges Signal.
//     - Wenn -1: nur Software-Heuristik (Spannungsanstieg ueber ~30 s).
//   Hardware-Mod (siehe README.md, Abschnitt "Loetmod: VBUS auf GPIO"):
//     USB-VBUS (5 V) -> 10 kOhm -> GPIO16 -> 10 kOhm -> GND
#define PIN_VBUS_DET   16

// ---- BLE / Sources ----
#define MAX_SOURCES         16
#define SOURCE_TIMEOUT_MS   5000
// Zweistufige RSSI-Mittelung:
//   1) Alle Samples einer RSSI_BIN_MS-Periode -> Sekunden-Mittelwert
//   2) Moving Average ueber die letzten RSSI_AVG_SECS Sekunden-Mittelwerte
// Gesamt-Mittelungszeit = RSSI_BIN_MS * RSSI_AVG_SECS / 1000 Sekunden.
// Anzeige aktualisiert sich nur bei Bin-Abschluss (1x pro Sekunde).
#define RSSI_BIN_MS         1000
#define RSSI_AVG_SECS       3

// ---- Buttons ----
#define BTN_DEBOUNCE_MS     30
#define BTN_LONG_MS         1500

// ---- Loop-Timings ----
#define DISPLAY_FPS_MS      100      // 10 Hz
#define BAT_SAMPLE_MS       2000
#define MENU_TIMEOUT_MS     20000

// ---- UI ----
#define MENU_ITEMS          5

// ---- Audio (LEDC PWM auf PAM8403) ----
#define AUDIO_LEDC_FREQ     4000
#define AUDIO_LEDC_RES      8
#define AUDIO_CLICK_MS      2

// ---- Dose-Modell (Pfadverlust + Abstandsquadratgesetz) ----
//   d/d_peak = 10^(-deltaRSSI / (10*n))    Pfadverlust
//   I/I_max  = (1/d)^2 = 10^(deltaRSSI / (5*n))  Abstandsquadratgesetz
// Effektiver Falloff = 5*n dB pro Dekade Intensitaet:
//   n=2..4   physikalisch (Free Space ... Indoor) - im Training zu aggressiv
//   n=8      sanft, 1 m ~30%
//   n=12     sehr sanft, 1 m ~45% (Training-Default)
#define DOSE_PATH_LOSS_N    12.0f
