# CBRN-RN Sonde

Trainings-Sonde fuer ein CBRN-RN-Demonstrator-Set. Empfaengt per BLE-Advertising
die Datenpakete simulierter Strahlungsquellen ("Strahler-NNN") und stellt die
ortsabhaengig berechnete Dosisleistung als Analog-Gauge + Digitalwert dar.

Hardware: **LilyGO T-Display-S3 AMOLED** (ESP32-S3 R8, 1.91" RM67162 QSPI, 536x240).

---

## Funktionen

- BLE-Scan auf 1M-PHY (Normal) und Coded-PHY/LR (Umschaltbar im Menue — siehe TODO unten)
- Filter auf Geraete-Namen `Strahler-NNN`, anschliessend Manufacturer-Data CRC-Pruefung
- Dosis-Berechnung aus Manufacturer-Data Werten + RSSI-Pfadverlust + Abstandsquadratgesetz
- Auto-Range-Anzeige (nSv/uSv/mSv-Skala je nach `max_dosis` der gewaehlten Quelle)
- Beste Quelle = max(berechnete Dosis) ueber alle empfangenen Quellen
- Geiger-Klick-Audio (LEDC PWM, Klick-Rate proportional zur Dosis)
- Akku-Anzeige mit %-Wert + USB-Lade-Erkennung
- Menue mit: Reichweite-Toggle / Audio-Toggle / Quellen-Liste / Besitzer-Info / Aus-Modus
- Deep Sleep ("Aus") mit Wake-Up auf HW-Taste
- Version unten rechts auf jedem Screen

---

## Verkabelung

| Funktion        | GPIO     | Anmerkung                                       |
|-----------------|----------|-------------------------------------------------|
| Display QSPI    | 5,6,7,17,18,47,48 | onboard, nicht aendern                  |
| HW-Taste (Boot) | 0        | onboard, active low                             |
| Taste A         | 12       | extern, Taster gegen GND (INPUT_PULLUP intern)  |
| Taste B         | 13       | extern, Taster gegen GND (INPUT_PULLUP intern)  |
| Audio out       | 14       | -> PAM8403 IN+ (ggf. RC-Filter 1 kΩ / 100 nF)   |
| Lautsprecher    | -        | 8 Ω / 1 W direkt an PAM8403 OUT                  |
| Batterie        | 4        | onboard `BAT_VOLT`, Spannungsteiler 1:2          |

---

## Bedienung

| Screen       | HW kurz                  | HW lang        | A kurz                 | B kurz                  |
|--------------|--------------------------|----------------|------------------------|-------------------------|
| **Gauge**    | Menue oeffnen            | Aus-Modus      | Audio toggle           | Reichweite toggle       |
| **Menue**    | Auswahl bestaetigen      | zurueck zur Gauge | Auswahl hoch           | Auswahl runter          |
| **Quellen**  | zurueck zum Menue        | zurueck zum Menue | naechste Quelle        | vorherige Quelle        |
| **Besitzer** | zurueck zum Menue        | zurueck zum Menue | zurueck zum Menue      | zurueck zum Menue       |

Inaktivitaets-Timeout im Menue: 20 s ohne Eingabe -> zurueck zur Gauge.

---

## BLE-Protokoll der Strahler

**Geraetename:** `Strahler-NNN` (NNN = Sender-Nummer 000–250)

**Manufacturer-Data** (im Advertising-Frame, kein GATT noetig):

| Byte | Inhalt                                                          |
|------|-----------------------------------------------------------------|
| 0–1  | `0xFF 0xFF` Company-ID (custom/Test)                            |
| 2    | `0x01` Protokoll-Version                                        |
| 3    | Sender-Nr (1–254)                                               |
| 4    | Nuklid-ID (1–8, siehe `nuklid_name[]` in `src/ble.cpp`)         |
| 5–6  | max. Dosisleistung in 0,01 µSv/h, Big Endian                    |
| 7    | TX-Power in dBm (int8) — derzeit +4                              |
| 8    | XOR-CRC ueber Byte 2–7                                          |

**Reichweiten-Modus** (Strahler-seitig):
- *Normal*: Legacy Advertising, 1 Mbps, ~30–50 m
- *Lang (LR)*: BLE 5 Extended Advertising, Coded PHY S=8, ~150–200 m

---

## Dosis-Modell

Aus dem RSSI-Differenz `Δ = rssi - rssi_peak` und der maximalen Dosisleistung
der Quelle wird der aktuelle Dosiswert nach folgendem Modell bestimmt:

```
d / d_peak  = 10^(-Δ / (10·n))         (Pfadverlust)
I / I_max   = (1/d)^2 = 10^(Δ / (5·n)) (Abstandsquadratgesetz)
```

Der **Pfadverlust-Exponent `n`** ist in `include/config.h` als
`DOSE_PATH_LOSS_N` konfigurierbar:

| n   | Umgebung                                | Anzeigeverlauf                 |
|-----|------------------------------------------|--------------------------------|
| 2.0 | Free Space (theoretisch)                | sehr schneller Abfall          |
| 3.0 | outdoor mit Bodenreflexion              | klassisches 1/d² Verhalten     |
| 4.0 | indoor mit Waenden / Multipath (Default)| traeger, fuer Trainings gut    |

`rssi_peak` ist der bisher hoechste RSSI dieser Quelle — `100 %`-Stellung der
Gauge entspricht "so nah, wie ich diese Quelle je gesehen habe".

---

## Code-Struktur

```
include/
  config.h     Pinout, Tuning-Konstanten, Version
  state.h      globale Enums + UI-State extern
  colors.h     RGB565 Farbpalette
  ble.h        Source-Struct + Scan-API
  dose.h       calcDose, autoRange, RangeResult
  audio.h      Klick-Generator
  battery.h    BatState + Sampler
  buttons.h    Debounce + Btn-Struct
  display.h    Canvas + Screen-Funktionen
  power.h      Deep Sleep
  input.h      Tasten-Routing pro Screen
  owner.h      Besitzer-Info-Daten

src/
  main.cpp     setup + loop (Orchestrator)
  state.cpp    Globals-Definitionen
  ble.cpp      NimBLE-Scan + ScanCB + Manufacturer-Data-Parsing
  dose.cpp     Pfadverlust + Auto-Range
  audio.cpp    LEDC PWM Klick
  battery.cpp  ADC-Sampling
  buttons.cpp  Debounce-Statemachine
  display.cpp  Gauge, Top-Bar, alle Screens
  power.cpp    ESP-Deep-Sleep
  input.cpp    Screen-abh. Tasten-Routing
  owner.cpp    OWNER_LINES Inhalt
```

Versionsangabe wird zentral in `include/config.h` ueber
`FIRMWARE_VERSION_MAJOR/MINOR/STR` gesetzt und unten rechts im Display
sowie im Bootscreen ausgegeben.

---

## Build & Flash

PlatformIO (VS Code):

```
pio run -t upload
pio device monitor -b 115200
```

Build-Flags (in `platformio.ini`):
- `qio_opi` Memory-Type fuer OPI-PSRAM (sonst kein 257 KB Canvas)
- `CONFIG_ESP_TASK_WDT_TIMEOUT_S=15` (Sicherheitspolster fuer NimBLE-Spitzen)
- `CONFIG_BT_NIMBLE_EXT_ADV=1` (Vorbereitung Coded PHY)
- `CONFIG_BT_NIMBLE_ROLE_OBSERVER=1`, alle anderen `=0` (spart Heap)

---

## Offene Punkte / TODO

- **LR-Mode-PHY-Switch:** Der Menue-Toggle setzt aktuell nur das Flag
  `currentBleMode = BLE_LR`, der NimBLE-Stack scannt jedoch weiterhin
  auf 1M-PHY. Der echte Switch auf Coded-PHY muss noch implementiert
  werden — exakte API in NimBLE-Arduino 2.x: vermutlich `setPhy()` oder
  via `enableExtendedScan() + setExtScanPhy()`. Siehe `src/ble.cpp`
  → `startScan()`.

- **Persistente Owner-Info:** `OWNER_LINES[]` in `src/owner.cpp` ist
  derzeit hartkodiert. Spaeter z.B. ueber NVS-Storage befuellbar machen.

- **Akku-Kalibrierung:** Die Lade-Erkennung basiert auf einer
  Spannungsschwelle (≥ 4.15 V = USB), weil der TP4056-CHRG-Pin am
  Board nicht zum ESP gefuehrt ist. Genauer wuerde ein dedizierter
  GPIO mit Pullup gegen CHRG funktionieren — falls Hardware angepasst
  werden kann.
