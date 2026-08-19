# CBRN-RN Sonde

Trainings-Sonde fuer ein CBRN-RN-Demonstrator-Set. Empfaengt per BLE-Advertising
die Datenpakete simulierter Strahlungsquellen ("Strahler-NNN") und stellt die
ortsabhaengig berechnete Dosisleistung als Analog-Gauge + Digitalwert dar.

Hardware: **LilyGO T-Display-S3 AMOLED** (ESP32-S3 R8, 1.91" RM67162 QSPI, 536x240).

---

## Funktionen

- BLE-Scan auf 1M-PHY (Normal) und Coded-PHY/LR (Umschaltbar im Menue)
- Filter auf Geraete-Namen `Strahler-NNN`, anschliessend Manufacturer-Data CRC-Pruefung
- Dosis-Berechnung aus Manufacturer-Data Werten + RSSI-Pfadverlust + Abstandsquadratgesetz
- Auto-Range-Anzeige (nSv/uSv/mSv/Sv-Skala je nach `max_dosis` der gewaehlten Quelle)
- Beste Quelle = max(berechnete Dosis) ueber alle empfangenen Quellen
  (auf gemeinsame Einheit uSv/h normalisiert, damit Sender mit unterschiedlichen
  Einheits-Indizes vergleichbar sind)
- Aufwaerm-/Kalibrierphase ~`RSSI_BIN_MS * RSSI_AVG_SECS` (3 s) nach Erstkontakt
  bzw. Wiederfund einer Quelle; Anzeige zeigt waehrenddessen "Kalibrierung..."
  statt eines noch nicht aussagekraeftigen Dosis-Wertes
- Geiger-Klick-Audio (LEDC PWM, Klick-Rate proportional zur Dosis)
- Akku-Anzeige mit %-Wert + USB-Lade-Erkennung
- Menue mit: Reichweite-Toggle / Audio-Toggle / Quellen-Liste / Besitzer-Info /
  WLAN-Update / Display-Ausrichtung / Aus-Modus
- WLAN-/Update-Portal (WPA2-AP `CBRN-Sonde-Setup`, http://192.168.4.1): Firmware
  per Browser hochladen (OTA) **und** alle Menue-Einstellungen (Reichweite, Audio,
  Display-Ausrichtung) setzen — persistent in NVS
- Einstellungen (Reichweite, Audio, Display-Ausrichtung) bleiben ueber Neustarts
  erhalten (NVS-Namespace `ui`)
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
| VBUS-Detect     | 16       | optional, siehe "Loetmod: VBUS auf GPIO" unten   |

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

**Manufacturer-Data** (im Advertising-Frame, kein GATT noetig, Advertising-Intervall 100 ms):

| Byte | Inhalt                                                          |
|------|-----------------------------------------------------------------|
| 0–1  | `0xFF 0xFF` Company-ID (custom/Test)                            |
| 2    | `0x02` Protokoll-Version                                        |
| 3    | Sender-Nr (1–254)                                               |
| 4    | Nuklid-ID (1–9, siehe `nuklid_name[]` in `src/ble.cpp`)         |
| 5–6  | max. Dosisleistung als Ganzzahl, Big Endian                     |
| 7    | Einheits-Index: 0=nSv/h, 1=uSv/h, 2=mSv/h, 3=Sv/h               |
| 8    | TX-Power in dBm (int8) — derzeit +4                              |
| 9    | XOR-CRC ueber Byte 2–8                                          |

Die Sonde normalisiert den max-Dosis-Wert beim Empfang mit dem Einheits-Index
auf uSv/h, damit Quellen mit unterschiedlichen Einheiten direkt verglichen
und in der gleichen Skala dargestellt werden koennen.

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
`DOSE_PATH_LOSS_N` konfigurierbar. Effektiv ergibt sich `5·n` dB RSSI-Drop
pro Dekade Dosis-Abfall — physikalische Werte (n=2..4) sind im
Trainings-Setting zu aggressiv, daher hoeher gewaehlt:

| n    | Anzeigeverlauf bei ~−20 dB RSSI-Drop (≈ 1 m)         |
|------|------------------------------------------------------|
| 2.0  | Free Space — sehr schneller Abfall (~1%)             |
| 4.0  | Indoor physikalisch — immer noch zu schnell (~9%)    |
| 8.0  | sanft (~30%)                                         |
| 12.0 | sehr sanft (~45%, Trainings-Default)                 |

`rssi_peak` ist der bisher hoechste RSSI dieser Quelle — `100 %`-Stellung der
Gauge entspricht "so nah, wie ich diese Quelle je gesehen habe".

---

## Loetmod: VBUS auf GPIO (Lade-Erkennung)

Der ESP32-S3 hat **keinen** internen VBUS-Sense-Pin, der nach aussen
gefuehrt waere, und der TP4056 auf der T-Display-S3 fuehrt seinen CHRG-Pin
nicht zum MCU. Die Firmware nutzt deshalb in der Default-Konfiguration eine
Software-Heuristik (Spannungsanstieg ueber ~30 s) — die ist aber traege und
erkennt das Einstecken erst nach einer halben Minute.

Mit einem kleinen Loetmod bekommt man sofortige, eindeutige USB-Erkennung:
ein Spannungsteiler von **USB-VBUS (5 V) auf GPIO16** liefert beim Stecken
~2.5 V (HIGH), beim Ziehen 0 V (LOW).

### Schaltung

```
   USB-VBUS (5 V)
        |
        |
       [R1]   10 kOhm
        |
        +-----------> GPIO16   (in der Firmware: PIN_VBUS_DET)
        |
       [R2]   10 kOhm
        |
       GND
```

R1 = R2 = 10 kOhm (1/8 W reicht; Toleranz 5 % unkritisch). Ergibt am
GPIO 5 V × 10 / (10+10) = **2.5 V**, sicher ueber dem HIGH-Threshold des
ESP32-S3 (~1.6 V) und unter VDD (3.3 V).

Stromaufnahme der Schaltung dauerhaft: 5 V / 20 kOhm = 250 µA — vernachlaessigbar.

### Anschlusspunkte auf dem Board

| Signal     | Anzapfen an                                                 |
|------------|-------------------------------------------------------------|
| USB-VBUS   | Pin 1/4 (oder A1/A4 / B1/B4) der USB-C-Buchse, **oder** das `5V`-Testpad neben dem TP4056, **oder** der Eingang des 3.3-V-LDO |
| GND        | beliebiges GND-Pad (am einfachsten der `GND`-Pin der Stiftleiste) |
| GPIO16     | Pin `IO16` der Stiftleiste                                  |

Praktisch: die beiden Widerstaende auf ein Stueck Lochraster oder direkt
fliegend an die Stiftleiste loeten, jeweils mit Schrumpfschlauch ueber den
Beinchen.

### Aktivieren in der Firmware

Bereits vorbereitet: in [include/config.h](include/config.h) ist
`PIN_VBUS_DET = 16` gesetzt und der Pin wird mit `INPUT_PULLDOWN`
initialisiert. Solange nichts angeloetet ist, liest GPIO16 stabil LOW —
die Software-Heuristik laeuft als Fallback weiter und nichts ist
schlimmer als vorher. Sobald die Bruecke verloetet ist, hat der Pin
Vorrang und USB wird sofort beim Einstecken erkannt.

Andere GPIO statt 16: in `config.h` `PIN_VBUS_DET` auf die gewuenschte
Nummer setzen. `-1` deaktiviert den HW-Pfad komplett (nur SW-Heuristik).

### Schnelltest nach dem Loeten

1. Geraet im Akku-Betrieb starten — Top-Bar zeigt nur `xx%`, kein `USB`.
2. USB-Kabel einstecken — innerhalb 1 s sollte `USB` (cyan) erscheinen.
3. Ziehen — `USB`-Label verschwindet innerhalb 1 s wieder.

Wenn das nicht klappt: GPIO16 mit Multimeter messen
(unverloetet ~0 V, mit Mod und USB ~2.5 V, mit Mod ohne USB ~0 V).

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
  owner.cpp    OwnerInfo-Struct mit Kontakt-/Besitzer-Daten
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

- **Persistente Owner-Info:** `OWNER` struct in `src/owner.cpp` ist
  derzeit hartkodiert. Spaeter z.B. ueber NVS-Storage befuellbar machen.

- **Lade-Erkennung (USB):** SW-Heuristik laeuft als Default-Fallback,
  GPIO16 ist als HW-VBUS-Pin vorbereitet (mit `INPUT_PULLDOWN`).
  Anleitung zum Loetmod siehe Abschnitt "Loetmod: VBUS auf GPIO".
