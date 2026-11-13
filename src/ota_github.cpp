#include "ota_github.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <time.h>

#include "config.h"
#include "github_ca.h"

// https://github.com/<owner>/<repo>/releases/latest/download/<bestand>
// GitHub stuurt dit door (302) naar het bestand in de nieuwste release.
static String latestAssetUrl(const char* bestand) {
  return String("https://github.com/") + GITHUB_OWNER + "/" + GITHUB_REPO +
         "/releases/latest/download/" + bestand;
}

// TLS-certificaten hebben een geldigheidsperiode: zet eerst de klok juist.
static bool syncClock() {
  if (time(nullptr) > 1700000000) return true;  // al gesynchroniseerd
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  for (int i = 0; i < 50; i++) {  // max. 5 s
    if (time(nullptr) > 1700000000) return true;
    delay(100);
  }
  return false;
}

static bool fetchLatestVersion(WiFiClientSecure& client, String& versie) {
  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  if (!http.begin(client, latestAssetUrl(OTA_VERSION_ASSET))) return false;

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] %s ophalen mislukt: HTTP %d\n", OTA_VERSION_ASSET, code);
    http.end();
    return false;
  }
  versie = http.getString();
  versie.trim();
  http.end();
  return versie.length() > 0 && versie.length() < 32;
}

void otaCheckForUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (strcmp(FIRMWARE_VERSION, "dev") == 0) {
    Serial.println("[OTA] Lokale dev-build: update-check overgeslagen.");
    return;
  }
  if (!syncClock()) {
    Serial.println("[OTA] Geen NTP-tijd, update-check overgeslagen.");
    return;
  }

  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CA);

  String nieuwste;
  if (!fetchLatestVersion(client, nieuwste)) return;

  if (nieuwste == FIRMWARE_VERSION) {
    Serial.printf("[OTA] Firmware is up-to-date (%s).\n", FIRMWARE_VERSION);
    return;
  }

  Serial.printf("[OTA] Nieuwe versie %s gevonden (huidig: %s). Downloaden...\n",
                nieuwste.c_str(), FIRMWARE_VERSION);
  digitalWrite(PIN_STATUS_LED, HIGH);

  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(true);
  httpUpdate.onProgress([](int klaar, int totaal) {
    static int vorige = -1;
    int pct = totaal > 0 ? (klaar * 100) / totaal : 0;
    if (pct / 10 != vorige / 10) {
      Serial.printf("[OTA] %d%%\n", pct);
      vorige = pct;
    }
  });

  t_httpUpdate_return r = httpUpdate.update(client, latestAssetUrl(OTA_FIRMWARE_ASSET));

  // Bij succes herstart de ESP32 al binnen update(); hier komen we alleen bij een fout.
  digitalWrite(PIN_STATUS_LED, LOW);
  if (r == HTTP_UPDATE_FAILED) {
    Serial.printf("[OTA] Update mislukt (%d): %s\n", httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
  }
}
