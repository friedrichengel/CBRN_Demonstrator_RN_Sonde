// ================================================================
// webportal.h - WLAN-Accesspoint + Webseite fuer Firmware-Update (OTA)
//
//   Wird ueber das Menue aktiviert. initWifiPortal() stoppt BLE, oeffnet
//   einen WPA2-gesicherten Accesspoint und startet einen Webserver unter
//   http://192.168.4.1 . Auf der Seite kann man:
//     - eine Firmware-.bin direkt hochladen (OTA in den inaktiven Slot)
//     - optional einem vorhandenen WLAN beitreten (Zugangsdaten -> NVS)
//   Captive-Portal-DNS leitet jede Anfrage auf die Startseite.
// ================================================================
#pragma once
#include <stdint.h>

enum OtaState { OTA_IDLE, OTA_RUNNING, OTA_SUCCESS, OTA_ERROR };

struct WifiStatus {
  char     ap_ssid[33];
  char     ap_ip[16];
  int      clients;          // verbundene AP-Clients
  bool     sta_active;       // Heimnetz-Verbindung angefordert?
  bool     sta_connected;    // Heimnetz verbunden?
  char     sta_ssid[33];
  char     sta_ip[16];
  OtaState ota;
  uint32_t ota_bytes;        // bisher geschriebene Bytes
  char     ota_msg[48];
};

void initWifiPortal();    // BLE stoppen, AP + Webserver starten
void handleWifiPortal();  // im loop aufrufen (DNS + Webserver bedienen)
void stopWifiPortal();    // Webserver/WiFi stoppen, BLE neu starten

const WifiStatus& wifiStatus();
