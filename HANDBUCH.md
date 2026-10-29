# Handbuch — CBRN-RN Sonde

**Trainings-Messgerät (Empfänger) für den CBRN-RN-Demonstrator**
Privates Ausbildungsprojekt · Firmware 2.21

> **Hinweis:** Dies ist ein **privates Projekt** und **kein offizielles Produkt
> oder Ausbildungsmittel des THW** (Bundesanstalt Technisches Hilfswerk). Es wird
> lediglich im Rahmen der Ausbildung eines Ortsverbands verwendet. „THW" ist ein
> Kennzeichen der Bundesanstalt Technisches Hilfswerk; es besteht keine
> offizielle Verbindung. Quelltext und Lizenz (MIT):
> <https://github.com/friedrichengel/CBRN_Demonstrator_RN_Sonde>

---

## 1. Was das Gerät ist und was es nicht ist

Die Sonde ist das **Anzeige- und Suchgerät** des Demonstrator-Sets. Sie empfängt
per Bluetooth die Funksignale der Trainings-Strahler und stellt eine daraus
berechnete **Dosisleistung** auf einem Rundinstrument dar — mit Geigerzähler-
Klickgeräuschen, Farbzonen und automatischer Einheitenwahl.

> **Wichtig:** Es wird **keine Strahlung gemessen und keine erzeugt.** Der
> angezeigte Wert wird ausschließlich aus der Funk-Signalstärke des jeweiligen
> Strahlers berechnet. Das Gerät ist ein **Ausbildungsmittel**, kein Dosimeter,
> und darf niemals für reale Messungen oder Lagebeurteilungen verwendet werden.

Die Anzeige verhält sich in der Ausbildung wie ein echtes Suchgerät: je näher
man dem Strahler kommt, desto höher der Wert und desto schneller die Klicks.

---

## 2. Bedienelemente

Das Gerät hat **drei Tasten**:

| Taste | Lage | Beschriftung im Handbuch |
|---|---|---|
| Hauptknopf | seitlich am Gehäuse (Boot-Taster des Boards) | **HW** |
| Taste A | externer Taster | **A** |
| Taste B | externer Taster | **B** |

Es gibt **zwei Tastengesten**:

| Geste | Definition |
|---|---|
| **kurz** | drücken und wieder loslassen |
| **lang** | mindestens **1,5 Sekunden** gedrückt halten |

Ein Tastendruck wird 30 ms entprellt — sehr kurzes Antippen wird ignoriert.

---

## 3. Einschalten und Ausschalten

**Einschalten:** HW-Taste einmal drücken. Es erscheint kurz der Startbildschirm
(„CBRN-RN / SONDE v2.21"), danach nach ca. einer Sekunde die Messanzeige.

**Ausschalten:** In der Messanzeige die **HW-Taste lang** drücken, oder im Menü
den Punkt **Aus-Modus** wählen. Das Display zeigt „Aus" und das Gerät geht in
den Tiefschlaf (Deep Sleep). Der Stromverbrauch ist dabei minimal.

**Wieder einschalten:** HW-Taste drücken.

Alle Einstellungen (Reichweite, Ton, Display-Ausrichtung) bleiben über das
Aus- und Wiedereinschalten hinweg gespeichert.

---

## 4. Die Messanzeige (Hauptbildschirm)

Der Bildschirm ist im Querformat aufgeteilt:

```
 [Akku] 87%  USB                                    AUD  LR
                          ____________
   12.4                 /   µSv/h      \
   µSv/h              /                  \
                     |     (Zeiger)       |
                      \                  /
                        \______________/
                                                          2.21
```

| Bereich | Bedeutung |
|---|---|
| **Oben links** | Akku-Symbol mit Füllstand und Prozentwert. Grün ab 50 %, gelb unter 50 %, rot unter 20 %. |
| **`USB`** (cyan) | erscheint, sobald ein USB-Kabel steckt und geladen wird. |
| **Oben rechts `AUD`** | hell = Klickton eingeschaltet, dunkelgrau = Ton aus. |
| **Oben rechts `1M` / `LR`** | aktueller Empfangsmodus: `1M` = Normal, `LR` (cyan) = Lange Reichweite. |
| **Große Zahl links** | aktuelle Dosisleistung, darunter die Einheit. |
| **Rundinstrument** | Zeiger auf demselben Wert, Skala mit Farbzonen. |
| **Unten rechts** | Firmware-Version. |

### Farbzonen von Zeiger und Zahl

Die Farbe richtet sich nach dem Anteil am Skalen-Vollausschlag:

| Anteil der Skala | Farbe |
|---|---|
| unter 50 % | grün |
| 50 % bis 75 % | gelb |
| 75 % bis 90 % | orange |
| ab 90 % | rot |

### Automatische Skalierung

Skalenendwert und Einheit richten sich nach der **maximalen Dosisleistung, die
der jeweilige Strahler ausgibt**. Das Gerät wählt die Einheit selbstständig so,
dass der Wert gut lesbar ist:

| Vollausschlag des Strahlers | angezeigte Einheit |
|---|---|
| unter 1 µSv/h | nSv/h |
| 1 bis 1 000 µSv/h | µSv/h |
| 1 000 µSv/h bis 1 Sv/h | mSv/h |
| ab 1 Sv/h | Sv/h |

Vollausschlag (Zeiger ganz rechts) bedeutet: „so nah an dieser Quelle, wie das
Gerät sie bisher überhaupt gesehen hat".

### Anzeige `----` (Kalibrierung)

Nach dem Einschalten oder wenn ein Strahler neu in Reichweite kommt, braucht
die Sonde **etwa 3 Sekunden**, um die Signalstärke zu mitteln. Solange erscheint
statt eines Zahlenwertes ein gelbes **`----`**. Das ist normal und kein Fehler —
einfach kurz stehen bleiben, bis der Wert erscheint.

### Mehrere Strahler

Die Sonde kann bis zu **16 Strahler gleichzeitig** verfolgen. Angezeigt wird
immer **derjenige mit der höchsten aktuellen Dosisleistung**, also die für den
Erkunder relevanteste Quelle. Ein Strahler gilt nach **5 Sekunden ohne Empfang**
als verschwunden und fällt aus der Bewertung.

### Tasten in der Messanzeige

| Taste | Wirkung |
|---|---|
| **HW kurz** | Menü öffnen |
| **HW lang** | Gerät ausschalten |
| **A kurz** | Klickton an/aus |
| **B kurz** | Reichweite umschalten (Normal ⇄ Lang) |

---

## 5. Das Menü

**Öffnen:** in der Messanzeige **HW kurz** drücken.

| Taste | Wirkung im Menü |
|---|---|
| **A kurz** | Auswahl eine Zeile nach oben |
| **B kurz** | Auswahl eine Zeile nach unten |
| **HW kurz** | ausgewählten Punkt bestätigen bzw. umschalten |
| **HW lang** | zurück zur Messanzeige |

Das Menü läuft oben und unten zyklisch durch. Nach **20 Sekunden ohne
Tastendruck** springt das Gerät automatisch zurück zur Messanzeige.

### 5.1 Übersicht der Menüpunkte

| # | Menüpunkt | Anzeige | Wirkung bei HW kurz |
|---|---|---|---|
| 1 | **Reichweite** | `Normal` / `Lang (LR)` | schaltet sofort um, Empfang startet neu |
| 2 | **Audio** | `AN` / `AUS` | schaltet den Klickton um |
| 3 | **Quellen-Liste** | — | öffnet die Diagnose-Ansicht (PIN nötig) |
| 4 | **Besitzer-Info** | — | zeigt die Kontaktdaten des Ortsverbands |
| 5 | **WLAN / Update** | — | startet das WLAN-Portal (Firmware & Einstellungen) |
| 6 | **Display** | `Normal` / `180 Grad` | dreht das Bild um 180° |
| 7 | **Aus-Modus** | — | schaltet das Gerät aus |

### 5.2 Reichweite

Bestimmt, auf welchem Funkverfahren die Sonde lauscht. **Sonde und Strahler
müssen im selben Modus stehen**, sonst wird gar nichts empfangen.

| Modus | Anzeige oben rechts | typische Reichweite im Freien |
|---|---|---|
| **Normal** | `1M` | ca. 30–50 m |
| **Lang (LR)** | `LR` | ca. 150–200 m |

Umschalten geht auch direkt aus der Messanzeige heraus mit **B kurz**. Die
Einstellung wird gespeichert und gilt auch nach dem Neustart.

### 5.3 Audio

Schaltet die Geigerzähler-Klicks ein und aus. Die Klickrate steigt mit der
angezeigten Dosisleistung: eine kleine Grundrate ist immer hörbar, bei hoher
Dosis werden es bis zu 50 Klicks pro Sekunde. Die Klicks haben einen leichten
Zufallsanteil, damit sie natürlich klingen.

Umschalten geht auch direkt aus der Messanzeige heraus mit **A kurz**.

### 5.4 Quellen-Liste (Diagnose-Ansicht)

Diese Ansicht zeigt die technischen Details jeder empfangenen Quelle. Sie
verrät die Simulationsmechanik und ist deshalb **durch eine PIN geschützt**.

**PIN-Eingabe:** Nach Auswahl von „Quellen-Liste" erscheint eine vierstellige
Eingabemaske.

| Taste | Wirkung |
|---|---|
| **A kurz** | Ziffer an der markierten Stelle um 1 erhöhen (0→1→…→9→0) |
| **B kurz** | zur nächsten Stelle springen (läuft zyklisch) |
| **HW kurz** | Eingabe bestätigen |
| **HW lang** | Abbrechen, zurück zum Menü |

Bei falscher PIN springt das Gerät kommentarlos zurück ins Menü.

Nach richtiger Eingabe ist der **Diagnose-Modus** bis zum nächsten Neustart
freigeschaltet. Er wird **absichtlich nicht gespeichert** — nach jedem
Aus-/Einschalten ist er wieder gesperrt.

**Was der Diagnose-Modus zusätzlich freischaltet:**

- In der Messanzeige erscheint rechts ein Infoblock mit Anzahl der Quellen,
  Name und Nuklid der nächstgelegenen Quelle sowie deren Signalstärke, bzw. die
  Hinweise „Kalibrierung…" oder „Kein Signal".
- Der Menüpunkt „Quellen-Liste" öffnet direkt die Liste, ohne erneute PIN.

**Bedienung der Quellen-Liste:**

| Taste | Wirkung |
|---|---|
| **A kurz** | nächste Quelle |
| **B kurz** | vorherige Quelle |
| **HW kurz oder lang** | zurück zum Menü |

Angezeigt werden pro Quelle: Zähler „Quelle x / y", Gerätename (`Strahler-NNN`),
Nuklid, Sender-Nummer, Sendeleistung, aktuelle Dosisleistung, maximale
Dosisleistung der Quelle sowie aktuelle und beste je gemessene Signalstärke.

Wird gar nichts empfangen, steht dort „Keine Quelle empfangen".

### 5.5 Besitzer-Info

Zeigt formatfüllend die Kontaktdaten des Ortsverbands: Name, Anschrift, Telefon,
Fax und E-Mail. **Jede beliebige Taste** führt zurück ins Menü.

### 5.6 WLAN / Update

Öffnet ein Wartungsportal. Beim Start **wird der Bluetooth-Empfang gestoppt** —
solange das Portal läuft, misst die Sonde nicht.

Auf dem Display stehen die Zugangsdaten:

| Feld | Wert |
|---|---|
| **Netzwerkname (SSID)** | `CBRN-Sonde-Setup` |
| **Passwort** | `sonde-update` |
| **Adresse im Browser** | `http://192.168.4.1` |

Vorgehen:

1. Am Handy oder Notebook mit dem WLAN `CBRN-Sonde-Setup` verbinden.
2. Browser öffnen und `http://192.168.4.1` aufrufen. Auf vielen Geräten öffnet
   sich die Seite automatisch als Anmeldeseite.
3. Auf der Seite stehen drei Bereiche zur Verfügung (siehe unten).

**Verlassen:** ausschließlich mit **HW lang**. Kurze Tastendrücke werden hier
bewusst ignoriert, damit ein laufendes Update nicht versehentlich abgebrochen
wird. Danach läuft der Bluetooth-Empfang automatisch wieder an.

#### Bereich „Firmware-Update"

`.bin`-Datei auswählen und hochladen. Während des Vorgangs zeigt das Display
den Fortschritt in Kilobyte und den Hinweis „Läuft – bitte nicht ausschalten!".
Nach erfolgreichem Update **startet das Gerät automatisch neu**.

> Während des eigentlichen Schreibvorgangs steht das Display kurz still. Der
> Fortschritt ist währenddessen im Browser sichtbar. Das ist normal.

#### Bereich „Einstellungen"

Dieselben Einstellungen wie im Gerätemenü, nur bequemer am Handy:

| Feld | Werte | Wirksam |
|---|---|---|
| Reichweite | Normal / Lang (LR) | erst beim Verlassen des WLAN-Portals |
| Audio | An / Aus | sofort |
| Display-Ausrichtung | Normal / 180 Grad | sofort |

Alle Werte werden dauerhaft gespeichert.

#### Bereich „Mit WLAN verbinden (optional)"

Trägt die Sonde zusätzlich in ein vorhandenes WLAN ein (z. B. das
Unterkunfts-WLAN). Netzwerkname aus der Vorschlagsliste wählen oder eintippen,
Passwort eingeben, speichern. Der Verbindungsstatus erscheint auf dem Display.
Das Update-Portal bleibt parallel unter `192.168.4.1` erreichbar. Die
Zugangsdaten werden gespeichert und beim nächsten Portalstart automatisch
verwendet.

### 5.7 Display

Dreht das Bild um 180°. Nützlich, wenn das Gerät kopfüber getragen oder
befestigt wird. Das Querformat bleibt erhalten. Die Einstellung wird gespeichert.

### 5.8 Aus-Modus

Schaltet das Gerät ab (siehe Abschnitt 3).

---

## 6. Akku und Laden

- Der Ladezustand wird oben links in Prozent angezeigt (grob geschätzt aus der
  Akkuspannung, nicht kalibriert).
- Bei angeschlossenem USB-Kabel erscheint zusätzlich `USB` in Cyan.
- Geladen wird über die USB-C-Buchse des Boards. Das Gerät kann während des
  Ladens normal benutzt werden.

> Je nach Ausführung erkennt die Sonde das Einstecken entweder sofort (bei
> Geräten mit nachgerüsteter VBUS-Leitung) oder erst nach etwa einer halben
> Minute (Erkennung über den Spannungsverlauf). Beides ist normal.

---

## 7. Typische Abläufe

### Gerät für eine Übung vorbereiten

1. Sonde einschalten, Akkustand prüfen.
2. Reichweite prüfen — sie muss zu der der Strahler passen (`1M` oder `LR`
   oben rechts).
3. Ton nach Bedarf einschalten (`AUD` hell).
4. Kurz neben einen aktiven Strahler gehen und prüfen, dass ein Wert erscheint.

### Eine Quelle suchen

1. Ton einschalten — die Klickrate ist beim Suchen aussagekräftiger als der
   Zahlenwert.
2. Langsam gehen und dabei die Richtung wechseln. Nach jeder Richtungsänderung
   ein paar Sekunden warten, da die Anzeige über 3 Sekunden mittelt.
3. Der Zeiger steigt, je näher man kommt. Vollausschlag heißt: näher als je
   zuvor bei dieser Quelle.

---

## 8. Störungen und Abhilfe

| Beobachtung | Mögliche Ursache und Abhilfe |
|---|---|
| Anzeige bleibt bei `----` | Kalibrierung läuft (ca. 3 s). Bleibt es dauerhaft, wird gerade keine Quelle empfangen. |
| Gar kein Ausschlag, kein Ton | Strahler ausgeschaltet, außer Reichweite, oder **Reichweiten-Modus von Sonde und Strahler passen nicht zusammen** (`1M` vs. `LR`). |
| Wert springt oder fällt langsam | Normal. Funkabschattung durch Körper, Wände oder Fahrzeuge. Die Anzeige mittelt bewusst über 3 Sekunden. |
| Kein Klickton | `AUD` oben rechts ist dunkelgrau → mit **A kurz** einschalten. |
| Zeiger geht nicht mehr auf Vollausschlag | Der Merkwert für „am nächsten dran" wird pro Quelle gehalten. Nach einem Neustart der Sonde wird er zurückgesetzt. |
| Bild steht auf dem Kopf | Menü → **Display** → auf `Normal` bzw. `180 Grad` stellen. |
| Gerät reagiert im WLAN-Modus nicht auf kurze Tastendrücke | So gewollt. Mit **HW lang** verlassen. |
| Display friert beim Firmware-Update kurz ein | Normal. Fortschritt im Browser beobachten. |
| Gerät lässt sich nicht einschalten | Akku leer — USB anschließen und einige Minuten laden. |

---

## 9. Technischer Anhang

*Dieser Abschnitt richtet sich an Ausbilder und die Gerätebetreuung.*

### 9.1 Hardware

| Merkmal | Wert |
|---|---|
| Board | LilyGO T-Display-S3 AMOLED (ESP32-S3) |
| Display | 1,91″ AMOLED, 536 × 240 Pixel, Querformat |
| Bedienung | Boot-Taster (HW) sowie zwei externe Taster A und B |
| Ton | PWM-Klickgeber auf Verstärkermodul mit Kleinlautsprecher |
| Funk | Bluetooth 5 (Empfang, passiver Scan) |
| Speicher | Einstellungen dauerhaft im internen Flash |
| Firmware | 2.21 |

### 9.2 Funkprotokoll der Strahler

Die Sonde wertet ausschließlich Advertising-Pakete aus; es wird keine
Verbindung aufgebaut. Gefiltert wird auf den Gerätenamen `Strahler-NNN`,
anschließend werden die Herstellerdaten auf Prüfsumme geprüft.

| Byte | Inhalt |
|---|---|
| 0–1 | Kennung `0xFF 0xFF` |
| 2 | Protokollversion `0x02` |
| 3 | Sender-Nummer (1–254) |
| 4 | Nuklid-Kennung (1–9) |
| 5–6 | max. Dosisleistung als Ganzzahl (Big Endian) |
| 7 | Einheiten-Index: 0 = nSv/h, 1 = µSv/h, 2 = mSv/h, 3 = Sv/h |
| 8 | Sendeleistung in dBm |
| 9 | Prüfsumme (XOR über Byte 2–8) |

Sendeintervall der Strahler: 100 ms.

**Nuklid-Kennungen:**

| ID | Nuklid | ID | Nuklid | ID | Nuklid |
|---|---|---|---|---|---|
| 1 | Cs-137 | 4 | Ir-192 | 7 | U-238 |
| 2 | Co-60 | 5 | Ra-226 | 8 | Pu-239 |
| 3 | Am-241 | 6 | Th-232 | 9 | Se-75 |

Die Sonde rechnet den Maximalwert beim Empfang anhand des Einheiten-Index auf
µSv/h um, damit Quellen mit unterschiedlichen Einheiten vergleichbar sind.

### 9.3 Rechenmodell der Dosisleistung

Aus der Differenz zwischen der aktuell geglätteten Signalstärke und der besten
je für diese Quelle gemessenen Signalstärke wird über ein Pfadverlustmodell
eine Entfernung und daraus über das Abstandsquadratgesetz ein Dosiswert
abgeleitet:

```
I / I_max = 10 ^ ( ΔRSSI / (5 · n) )      mit ΔRSSI = RSSI_aktuell − RSSI_beste
```

Der Pfadverlust-Exponent ist auf **n = 12** eingestellt. Physikalisch korrekte
Werte (n = 2 … 4) lassen die Anzeige in der Ausbildung viel zu schnell
abfallen; n = 12 ergibt einen sanften, gut spielbaren Verlauf.

### 9.4 Signalglättung und Zeitverhalten

| Größe | Wert |
|---|---|
| Mittelung Stufe 1 | Median aller Messwerte je 1-Sekunden-Fenster |
| Mittelung Stufe 2 | gleitender Mittelwert über 3 Sekundenwerte |
| Anzeige-Aktualisierung | 10 Bilder pro Sekunde, Messwert 1× pro Sekunde |
| Kalibrierdauer nach Erstkontakt | ca. 3 Sekunden |
| Timeout einer Quelle | 5 Sekunden ohne Empfang |
| maximale Anzahl Quellen | 16 |

### 9.5 Klickrate

```
Klicks pro Sekunde = 0,3 + 0,5 × Dosisleistung [µSv/h]     (begrenzt auf 50)
```

Die Abstände zwischen den Klicks werden zufällig variiert, um das Verhalten
eines echten Zählrohrs nachzubilden.

### 9.6 Gespeicherte Einstellungen

Dauerhaft gespeichert (überstehen Neustart und Firmware-Update):

- Reichweiten-Modus
- Ton an/aus
- Display-Ausrichtung
- WLAN-Zugangsdaten des optionalen Heimnetzes

Bewusst **nicht** gespeichert:

- Diagnose-Freischaltung (nach jedem Neustart wieder gesperrt)
- Bestwert der Signalstärke je Quelle

### 9.7 Diagnose-PIN

Die vierstellige PIN ist in der Firmware hinterlegt und für alle Geräte des
Sets gleich. Sie ist bei der Gerätebetreuung zu erfragen und sollte nicht an
Übungsteilnehmer weitergegeben werden. Da der Quelltext der Firmware
öffentlich ist, ist die PIN kein Sicherheitsmerkmal, sondern nur eine
Hemmschwelle. Wer ein eigenes Set betreibt, kann sie in `include/config.h`
(`DIAG_PIN`) ändern und die Firmware neu bauen.

---

*Handbuch zur Firmware 2.21 · Privates Projekt, kein offizielles THW-Produkt ·
Quelltext: <https://github.com/friedrichengel/CBRN_Demonstrator_RN_Sonde> · Lizenz: MIT*
