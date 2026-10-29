# Sicherheitshinweise / Security Policy

## Geltungsbereich

Dieses Projekt ist ein **privates Ausbildungs-Hilfsmittel**. Die Firmware
verwandelt ein LilyGO T-Display-S3 AMOLED (ESP32-S3) in die Nachbildung eines
Strahlungs-Suchgeräts: Sie empfängt per Bluetooth Low Energy die
Advertising-Pakete der Trainings-Strahler und rechnet aus der Signalstärke
einen Anzeigewert aus. Das Gerät **misst keine Strahlung und erzeugt keine**.
Es steuert keine sicherheitsrelevanten Systeme. Internetzugriff besteht nur,
wenn der Benutzer das optionale WLAN-Portal bewusst mit einem Heimnetz
verbindet; die Firmware selbst baut keine ausgehenden Verbindungen auf.

Zugehörige Repositories mit eigener Security Policy:

- [CBRN_Demonstrator_RN_Probe](https://github.com/friedrichengel/CBRN_Demonstrator_RN_Probe) (Strahler-Firmware)
- [CBRN_Demonstrator_RN_App](https://github.com/friedrichengel/CBRN_Demonstrator_RN_App) (Android-App)

## Bekannte, bewusst akzeptierte Eigenschaften

- **Diagnose-PIN im Quelltext.** Die vierstellige PIN steht als `DIAG_PIN` in
  [include/config.h](include/config.h) und ist identisch mit der PIN der
  Android-App. Sie schützt lediglich die Anzeige der Simulationsmechanik
  (Quellen-Liste, RSSI-Werte, Nuklid-Namen) vor Übungsteilnehmern und ist
  **kein Sicherheitsmerkmal**. Wer den Quelltext liest, kennt die PIN. Wer sie
  für ein eigenes Geräte-Set ändern will, passt `DIAG_PIN` an und baut neu.
- **WLAN-Portal mit festem Passwort.** Das Update-Portal (Menüpunkt
  „WLAN / Update") öffnet einen WPA2-Accesspoint, dessen SSID und Passwort als
  `WIFI_AP_SSID` / `WIFI_AP_PASS` in [include/config.h](include/config.h)
  stehen und zusätzlich auf dem Display angezeigt werden. Wer mit dem AP
  verbunden ist, kann **ohne weitere Anmeldung** Firmware hochladen
  (OTA-Update) und Geräteeinstellungen ändern. Die Firmware wird dabei weder
  signiert noch auf Herkunft geprüft. Das Portal läuft nur, solange es im
  Gerätemenü aktiv ist, und stoppt währenddessen den BLE-Empfang. Für den
  Übungsbetrieb ist das gewollt: Updates sollen ohne Rechner und Kabel möglich
  sein. Schlimmstenfalls spielt jemand in Funkreichweite eine fremde Firmware
  auf ein Trainingsgerät.
- **Heimnetz-Zugangsdaten im Klartext.** Trägt man im Portal ein vorhandenes
  WLAN ein, speichert die Firmware SSID und Passwort unverschlüsselt im NVS des
  ESP32 (Namespace `wifi`). Sie sind mit physischem USB-Zugriff auslesbar.
  Bitte dafür kein WLAN mit schutzwürdigem Passwort verwenden.
- **Unsigniertes Advertising.** Die Sonde wertet die unverschlüsselten
  Advertising-Pakete der Strahler aus. Der XOR-Prüfwert in Byte 9 der
  Manufacturer-Data erkennt nur Übertragungsfehler, keine Manipulation. Ein
  beliebiger BLE-Sender kann Strahler-Pakete nachbilden und so einen falschen
  Übungswert anzeigen lassen. Die Sonde selbst bietet **keinen GATT-Dienst**
  an; sie ist reiner BLE-Observer. Der offene GATT-Dienst `0x1900` gehört zur
  Strahler-Firmware und ist dort dokumentiert.
- **USB-Bootloader ohne Authentifizierung.** Der ESP32-S3 nimmt per USB/UART
  beliebige Firmware an (esptool). Secure Boot und Flash-Verschlüsselung sind
  nicht aktiviert. Das entspricht dem Auslieferungszustand des Boards.

## Was dieses Projekt nicht enthält

- keine Zugangsdaten, Schlüssel oder Zertifikate (die oben genannten Werte
  sind Gerätekonstanten, keine Geheimnisse)
- keine Telemetrie, keine Cloud-Anbindung
- keine personenbezogenen Daten; die im Menüpunkt „Besitzer-Info" angezeigten
  Angaben in [src/owner.cpp](src/owner.cpp) sind die öffentlich bekannten
  Kontaktdaten eines Ortsverbands, kein Hinweis auf eine offizielle
  Herausgeberschaft

## Meldung von Schwachstellen

Wenn du dennoch ein Problem findest, das über die oben genannten Punkte
hinausgeht, melde es bitte über ein
[GitHub-Issue](https://github.com/friedrichengel/CBRN_Demonstrator_RN_Sonde/issues)
oder, falls es vertraulich sein soll, über die GitHub-Funktion
„Report a vulnerability" (Security Advisories) dieses Repositories.
