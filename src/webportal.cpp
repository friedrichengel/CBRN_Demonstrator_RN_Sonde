// ================================================================
// webportal.cpp - WLAN-AP + Webserver + OTA-Firmware-Update
//
//   Verwendet nur Arduino-ESP32-Bordmittel (WiFi, WebServer, DNSServer,
//   Update, Preferences) - keine zusaetzlichen lib_deps.
//
//   Ablauf:
//     initWifiPortal()  -> BLE aus, AP (+optional STA) hoch, Webserver an
//     handleWifiPortal()-> DNS + Webserver bedienen, STA-Status pflegen
//     stopWifiPortal()  -> WiFi aus, BLE wieder an
//
//   Hinweis OTA: Der synchrone WebServer verarbeitet den Datei-Upload
//   blockierend innerhalb eines handleClient()-Aufrufs. Das Display friert
//   daher waehrend des eigentlichen Flashens kurz ein - der Fortschritt ist
//   im Browser sichtbar. Nach Erfolg startet das Geraet automatisch neu.
// ================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "webportal.h"
#include "config.h"
#include "ble.h"
#include "state.h"
#include "audio.h"
#include "display.h"

static WebServer  server(WIFI_PORTAL_PORT);
static DNSServer  dns;
static Preferences prefs;
static WifiStatus st;
static bool       running   = false;
static String     scanList;            // <option>-Eintraege gefundener Netze

static const byte DNS_PORT = 53;

// ---------------------------------------------------------------- HTML
static String htmlHead(const char* title) {
  String h = F("<!DOCTYPE html><html lang='de'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'><title>");
  h += title;
  h += F("</title><style>"
    "body{font-family:sans-serif;margin:0;background:#111;color:#eee}"
    ".w{max-width:480px;margin:0 auto;padding:16px}"
    "h1{font-size:20px;color:#0bf;margin:8px 0}h2{font-size:16px;margin:0 0 8px}"
    "input,select,button{width:100%;box-sizing:border-box;padding:10px;margin:6px 0;"
    "font-size:16px;border-radius:6px;border:1px solid #444;background:#222;color:#eee}"
    "button{background:#0bf;color:#000;font-weight:bold;border:0;cursor:pointer}"
    ".card{background:#1c1c1c;border-radius:8px;padding:14px;margin:12px 0}"
    "label{display:block;font-size:13px;color:#aaa;margin-top:8px}"
    ".muted{color:#888;font-size:13px}.ok{color:#3c6}.err{color:#e55}"
    "</style></head><body><div class='w'>");
  return h;
}
static const char* HTML_FOOT = "</div></body></html>";

static void handleRoot() {
  String p = htmlHead("CBRN-Sonde Update");
  p += F("<h1>CBRN-RN Sonde</h1><p class='muted'>Firmware ");
  p += FIRMWARE_VERSION_STR;
  p += F("</p>");

  // Status
  p += F("<div class='card'><h2>Status</h2><p class='muted'>AP-IP: ");
  p += st.ap_ip;
  if (st.sta_connected) {
    p += F("<br>WLAN: ");
    p += st.sta_ssid;
    p += F(" (");
    p += st.sta_ip;
    p += F(")");
  }
  p += F("</p></div>");

  // OTA-Upload
  p += F("<div class='card'><h2>Firmware-Update</h2>"
    "<p class='muted'>.bin-Datei waehlen und hochladen. Geraet startet danach neu.</p>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='firmware' accept='.bin' required>"
    "<button type='submit'>Update hochladen</button></form></div>");

  // Geraete-Einstellungen (identisch zum Menue auf dem Geraet)
  p += F("<div class='card'><h2>Einstellungen</h2>"
    "<p class='muted'>Werden sofort gespeichert. Der Reichweiten-Modus wird beim "
    "Verlassen des WLAN-Portals aktiv.</p>"
    "<form method='POST' action='/settings'>"
    "<label>Reichweite</label><select name='ble'>");
  p += currentBleMode == BLE_LR
       ? F("<option value='0'>Normal</option><option value='1' selected>Lang (LR)</option>")
       : F("<option value='0' selected>Normal</option><option value='1'>Lang (LR)</option>");
  p += F("</select><label>Audio</label><select name='audio'>");
  p += audioEnabled
       ? F("<option value='1' selected>An</option><option value='0'>Aus</option>")
       : F("<option value='1'>An</option><option value='0' selected>Aus</option>");
  p += F("</select><label>Display-Ausrichtung</label><select name='flip'>");
  p += displayFlipped
       ? F("<option value='0'>Normal</option><option value='1' selected>180 Grad</option>")
       : F("<option value='0' selected>Normal</option><option value='1'>180 Grad</option>");
  p += F("</select><button type='submit'>Einstellungen speichern</button></form></div>");

  // WLAN beitreten
  p += F("<div class='card'><h2>Mit WLAN verbinden (optional)</h2>"
    "<form method='POST' action='/wifi'>"
    "<input list='nets' name='ssid' placeholder='Netzwerkname (SSID)' required>"
    "<datalist id='nets'>");
  p += scanList;
  p += F("</datalist>"
    "<input type='password' name='pass' placeholder='Passwort'>"
    "<button type='submit'>Verbinden &amp; speichern</button></form></div>");

  p += HTML_FOOT;
  server.send(200, "text/html", p);
}

// ---- OTA-Upload-Handler (wird je Chunk aufgerufen) ----
static void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  switch (up.status) {
    case UPLOAD_FILE_START: {
      st.ota = OTA_RUNNING;
      st.ota_bytes = 0;
      snprintf(st.ota_msg, sizeof(st.ota_msg), "Start: %s", up.filename.c_str());
      Serial.printf("[OTA] Start: %s\n", up.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
        st.ota = OTA_ERROR;
        strncpy(st.ota_msg, "Fehler: begin()", sizeof(st.ota_msg) - 1);
      }
      break;
    }
    case UPLOAD_FILE_WRITE: {
      esp_task_wdt_reset();   // langer Upload -> WDT bei Laune halten
      if (st.ota == OTA_RUNNING) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
          Update.printError(Serial);
          st.ota = OTA_ERROR;
          strncpy(st.ota_msg, "Fehler: write()", sizeof(st.ota_msg) - 1);
        } else {
          st.ota_bytes += up.currentSize;
          snprintf(st.ota_msg, sizeof(st.ota_msg), "%u KB empfangen",
                   (unsigned)(st.ota_bytes / 1024));
        }
      }
      break;
    }
    case UPLOAD_FILE_END: {
      if (st.ota == OTA_RUNNING && Update.end(true)) {
        st.ota = OTA_SUCCESS;
        snprintf(st.ota_msg, sizeof(st.ota_msg), "OK: %u KB",
                 (unsigned)(up.totalSize / 1024));
        Serial.printf("[OTA] Erfolg: %u Bytes\n", (unsigned)up.totalSize);
      } else if (st.ota != OTA_ERROR) {
        Update.printError(Serial);
        st.ota = OTA_ERROR;
        strncpy(st.ota_msg, "Fehler: end()", sizeof(st.ota_msg) - 1);
      }
      break;
    }
    default:
      st.ota = OTA_ERROR;
      strncpy(st.ota_msg, "Upload abgebrochen", sizeof(st.ota_msg) - 1);
      break;
  }
}

// ---- Antwortseite nach Upload (Upload bereits abgeschlossen) ----
static void handleUpdateDone() {
  bool ok = (st.ota == OTA_SUCCESS);
  String p = htmlHead(ok ? "Update OK" : "Update fehlgeschlagen");
  if (ok) {
    p += F("<div class='card'><h2 class='ok'>Update erfolgreich</h2>"
      "<p>Das Geraet startet jetzt mit der neuen Firmware neu.</p></div>");
  } else {
    p += F("<div class='card'><h2 class='err'>Update fehlgeschlagen</h2><p>");
    p += st.ota_msg;
    p += F("</p><p><a href='/' style='color:#0bf'>Zurueck</a></p></div>");
  }
  p += HTML_FOOT;
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", p);
  if (ok) {
    delay(800);
    ESP.restart();
  }
}

// ---- WLAN-Beitritt: Zugangsdaten speichern + Verbindung starten ----
static void connectSta(const String& ssid, const String& pass) {
  st.sta_active = true;
  st.sta_connected = false;
  strncpy(st.sta_ssid, ssid.c_str(), sizeof(st.sta_ssid) - 1);
  st.sta_ssid[sizeof(st.sta_ssid) - 1] = 0;
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[WiFi] Verbinde mit '%s'...\n", ssid.c_str());
}

static void handleWifiSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (ssid.length() == 0) {
    server.send(400, "text/html", htmlHead("Fehler") +
      F("<p class='err'>SSID fehlt.</p><a href='/' style='color:#0bf'>Zurueck</a>") + HTML_FOOT);
    return;
  }
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();

  String p = htmlHead("Verbinde");
  p += F("<div class='card'><h2>Verbinde mit '");
  p += ssid;
  p += F("'</h2><p class='muted'>Zugangsdaten gespeichert. Status auf dem Geraet pruefen. "
    "Das Update-Portal bleibt unter der AP-IP erreichbar.</p>"
    "<p><a href='/' style='color:#0bf'>Zur Startseite</a></p></div>");
  p += HTML_FOOT;
  server.send(200, "text/html", p);

  connectSta(ssid, pass);
}

// ---- Geraete-Einstellungen speichern (wie im Menue, persistiert in NVS) ----
static void handleSettingsSave() {
  if (server.hasArg("ble"))
    setBleModeStored(server.arg("ble").toInt() == 1 ? BLE_LR : BLE_NORMAL);
  if (server.hasArg("audio"))
    setAudioEnabled(server.arg("audio").toInt() == 1);
  if (server.hasArg("flip"))
    setDisplayFlipped(server.arg("flip").toInt() == 1);

  String p = htmlHead("Gespeichert");
  p += F("<div class='card'><h2 class='ok'>Einstellungen gespeichert</h2>"
    "<p class='muted'>Audio und Display-Ausrichtung sind sofort aktiv. Der "
    "Reichweiten-Modus wird beim Verlassen des WLAN-Portals uebernommen.</p>"
    "<p><a href='/' style='color:#0bf'>Zur Startseite</a></p></div>");
  p += HTML_FOOT;
  server.send(200, "text/html", p);
}

// ---- Captive-Portal: alles Unbekannte auf die Startseite ----
static void handleNotFound() {
  server.sendHeader("Location", String("http://") + st.ap_ip, true);
  server.send(302, "text/plain", "");
}

// ---------------------------------------------------------------- API
void initWifiPortal() {
  if (running) return;

  memset(&st, 0, sizeof(st));
  st.ota = OTA_IDLE;
  strncpy(st.ota_msg, "Bereit", sizeof(st.ota_msg) - 1);

  // BLE abschalten -> Radio + Heap fuer WLAN frei
  stopBle();
  delay(50);

  // AP (+STA fuer optionales Beitreten/Scannen) hochfahren
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL);
  delay(100);
  IPAddress ip = WiFi.softAPIP();
  strncpy(st.ap_ssid, WIFI_AP_SSID, sizeof(st.ap_ssid) - 1);
  snprintf(st.ap_ip, sizeof(st.ap_ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  Serial.printf("[WiFi] AP '%s' -> http://%s\n", st.ap_ssid, st.ap_ip);

  // Netze scannen -> Datalist fuer die Beitritts-Maske
  scanList = "";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 15; i++) {
    scanList += "<option value='";
    scanList += WiFi.SSID(i);
    scanList += "'>";
  }
  Serial.printf("[WiFi] %d Netze gefunden\n", n);

  // Falls bereits ein Heimnetz gespeichert wurde -> automatisch verbinden
  prefs.begin("wifi", true);
  String savedSsid = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  prefs.end();
  if (savedSsid.length() > 0) connectSta(savedSsid, savedPass);

  // Captive-Portal-DNS: alle Hostnamen auf die AP-IP
  dns.start(DNS_PORT, "*", ip);

  // Webserver-Routen
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_POST, handleSettingsSave);
  server.on("/wifi", HTTP_POST, handleWifiSave);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound(handleNotFound);
  server.begin();

  running = true;
}

void handleWifiPortal() {
  if (!running) return;
  dns.processNextRequest();
  server.handleClient();

  // STA-Verbindungsstatus pflegen
  if (st.sta_active) {
    bool conn = (WiFi.status() == WL_CONNECTED);
    if (conn && !st.sta_connected) {
      IPAddress lip = WiFi.localIP();
      snprintf(st.sta_ip, sizeof(st.sta_ip), "%u.%u.%u.%u",
               lip[0], lip[1], lip[2], lip[3]);
      Serial.printf("[WiFi] verbunden, IP %s\n", st.sta_ip);
    }
    st.sta_connected = conn;
  }

  st.clients = WiFi.softAPgetStationNum();
}

void stopWifiPortal() {
  if (!running) return;
  running = false;
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("[WiFi] Portal beendet");
  delay(50);
  initBle();   // BLE-Scan wieder aufnehmen
}

const WifiStatus& wifiStatus() { return st; }
