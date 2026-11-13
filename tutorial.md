# Wi-Fi provisioning met een captive portal en Preferences op de ESP32

**Board:** DFRobot FireBeetle 2 ESP32-E · **Omgeving:** PlatformIO + Arduino-framework

In beginnersvoorbeelden staan de netwerkgegevens bijna altijd vast in de code:

```cpp
WiFi.begin("MijnWiFi", "Wachtwoord123");
```

Zodra je een apparaat aan iemand anders geeft, of thuis een nieuw modem krijgt, werkt het niet meer. In deze tutorial bouw je een apparaat dat:

- zelf een **setup-netwerk** uitzendt wanneer je een knop indrukt;
- via een **captive portal** (een pagina die vanzelf opent op je smartphone) Wi-Fi en extra instellingen vraagt;
- die instellingen **permanent bewaart** in het flashgeheugen met `Preferences.h`;
- daarna een sensor uitleest en de onboard RGB LED laat knipperen, met tijden die je via het portal instelt;
- zichzelf **draadloos bijwerkt** (OTA) zodra je een nieuwe versie op GitHub publiceert.

---

## 1. Wat is Wi-Fi provisioning?

**Wi-Fi provisioning** is het proces waarbij een apparaat zonder netwerk voor het eerst de netwerknaam (SSID) en het wachtwoord krijgt. De meest gebruiksvriendelijke manier is een **captive portal**: een configuratiepagina die automatisch verschijnt zodra je verbindt, zoals je kent van hotels en treinstations.

### Zo werkt het op de ESP32

1. **Opstarten:** de ESP32 leest de laatst opgeslagen Wi-Fi-gegevens uit het flashgeheugen (NVS).
2. **Knop ingedrukt? → Access Point-modus:** de ESP32 zendt zelf een netwerk uit (`FireBeetle-Setup`).
3. **DNS-omleiding:** een kleine DNS-server op de ESP32 stuurt élk webadres door naar zijn eigen IP-adres (`192.168.4.1`).
4. **Pop-up:** je smartphone merkt dat er geen internet is en opent automatisch de configuratiepagina.
5. **Opslaan:** je kiest je netwerk, typt het wachtwoord, vult extra velden in en klikt op *Save*. De ESP32 bewaart alles en verbindt als gewone client (Station-modus) met je netwerk.
6. **Geen knop ingedrukt?** Dan verbindt de ESP32 stil met het bekende netwerk, zonder zelf iets uit te zenden.

---

## 2. Twee soorten configuratie

| Soort | Waar? | Voorbeelden | Verandert tijdens gebruik? |
|---|---|---|---|
| **Compile-time constanten** | `include/config.h` met `constexpr` | GPIO-pinnen, baudrate, NVS-sleutelnamen, standaardwaarden | Nee, alleen door opnieuw te flashen |
| **Runtime configuratie** | NVS-flash via `Preferences.h` | Wi-Fi-gegevens, meetinterval, knippertijden | Ja, via het captive portal |

We gebruiken `constexpr` in plaats van `#define`: dat is type-veilig en voorkomt subtiele fouten.

Beide werelden werken samen: de `constexpr`-standaardwaarden uit `config.h` dienen als **fallback** wanneer er bij de allereerste opstart nog niets in het flashgeheugen staat.

---

## 3. Waarom alleen een Access Point via een fysieke knop?

Als de ESP32 bij elke opstart, of bij elke Wi-Fi-hapering, zelf een netwerk uitzendt, krijg je twee problemen:

- **Beveiliging:** iedereen in de buurt kan verbinden en je instellingen overschrijven.
- **Stroomverbruik:** een Access Point verbruikt fors meer stroom dan een slapende of verbonden ESP32. Een batterij is zo leeg.

De oplossing is **push-button provisioning**, zoals bij slimme stekkers en laadpalen: het setup-netwerk start alleen als de gebruiker bewust een knop indrukt.

> **Let op:** de WiFiManager-functie `autoConnect()` start standaard *automatisch* een open setup-netwerk (zonder wachtwoord) als verbinden mislukt. Dat schakelen we in de code bewust uit met `setEnableConfigPortal(false)`.

---

## 4. De hardware: FireBeetle 2 ESP32-E

| Functie | GPIO | Opmerking |
|---|---|---|
| Status-LED (onboard, blauw) | **2** | `LED_BUILTIN`, gewone `digitalWrite()` |
| CONFIG-knop (onboard user-knop) | **27** | Actief LOW (`INPUT_PULLUP`). Geen strapping-pin, dus mag ingedrukt zijn tijdens reset |
| RGB LED (onboard WS2812) | **5** | Data-LED: aansturen met `neopixelWrite()`, **niet** met `digitalWrite()` |
| Analoge sensor | **36** (`A0`) | Alleen ingang |

> **Waarom niet de BOOT-knop (GPIO 0)?** GPIO 0 is een *strapping-pin*: is die LOW tijdens een reset, dan start de ESP32 in **download-modus** in plaats van je programma. "Houd BOOT ingedrukt tijdens het opstarten" werkt daarom niet. De FireBeetle 2 ESP32-E heeft bovendien een aparte user-knop op GPIO 27.

---

## 5. Geheugen: RAM, flash en NVS

| Geheugen | Eigenschap | Wat staat erin? |
|---|---|---|
| **RAM (SRAM)** | Snel, maar **vluchtig**: weg bij stroomverlies, reset of deep sleep | Je gewone variabelen (`int meetInterval = 30;`) |
| **Flash** (4 MB) | **Permanent** | Je gecompileerde programma (`firmware.bin`) |
| **NVS** | Een partitie *in* het flash voor sleutel-waardeparen | Je instellingen via `Preferences.h` |

### Waarom `Preferences.h` en niet `EEPROM.h`?

Bij een Arduino Uno beheer je met `EEPROM.h` zelf geheugenadressen (`0x00`, `0x04`, …). Dat is erg foutgevoelig. `Preferences.h` werkt met:

- **Namespaces** (een soort map, bv. `"app-config"`);
- **Keys** met een duidelijke naam (bv. `"interval"`).

De ESP32 regelt de fysieke opslag zelf.

> **Let op:** een namespace en een key mogen elk **maximaal 15 tekens** lang zijn.

---

## 6. De volledige code

### 6.1 `platformio.ini`

```ini
[env:dfrobot_firebeetle2_esp32e]
platform = espressif32@6.5.0
board = dfrobot_firebeetle2_esp32e
framework = arduino
monitor_speed = 115200
lib_deps =
    tzapu/WiFiManager@^2.0.17
```

### 6.2 `include/config.h`: compile-time constanten

```cpp
#pragma once

#include <Arduino.h>

// =================================================================
// 1. HARDWARE CONSTANTEN (compile-time)
// =================================================================
constexpr uint8_t PIN_BUTTON_CONFIG = 27;           // Onboard user-knop (GPIO 27, actief LOW)
constexpr uint8_t PIN_STATUS_LED    = LED_BUILTIN;  // Onboard LED (GPIO 2 op FireBeetle 2 ESP32-E)
constexpr uint8_t PIN_SENSOR_ANALOG = A0;           // A0 = GPIO 36
constexpr uint8_t PIN_RGB_LED       = 5;            // Onboard WS2812 RGB LED (GPIO 5)

// =================================================================
// 2. COMMUNICATIE / PORTAL CONSTANTEN
// =================================================================
constexpr uint32_t SERIAL_BAUD_RATE      = 115200;
constexpr uint16_t AP_TIMEOUT_SEC        = 180;   // Max 3 minuten in setup-modus
constexpr uint16_t WIFI_CONNECT_TIMEOUT  = 10;    // Seconden
constexpr const char* AP_SSID_NAME       = "FireBeetle-Setup";
constexpr const char* AP_PASSWORD        = "supergeheim";  // min. 8 tekens

// Na reset heb je dit venster om de CONFIG-knop in te drukken (of je houdt hem al vast).
constexpr uint32_t CONFIG_WINDOW_MS      = 3000;
// Tijdens normaal bedrijf: knop zo lang ingedrukt houden -> portal.
constexpr uint32_t CONFIG_LONGPRESS_MS   = 5000;

// =================================================================
// 3. PREFERENCES NVS SLEUTELS EN DEFAULTS
// Namespace en key: elk MAXIMAAL 15 tekens!
// =================================================================
constexpr const char* NVS_NAMESPACE    = "app-config";
constexpr const char* NVS_KEY_INTERVAL = "interval";
constexpr const char* NVS_KEY_LED_ON   = "led_on";
constexpr const char* NVS_KEY_LED_OFF  = "led_off";

constexpr int DEFAULT_MEASURE_INTERVAL = 30;     // seconden
constexpr int MIN_MEASURE_INTERVAL     = 1;
constexpr int MAX_MEASURE_INTERVAL     = 86400;  // 1 dag

// RGB LED op GPIO 5: kleur (0-255 per kanaal) en knippertijden (seconden)
constexpr uint8_t RGB_COLOR_R = 0;
constexpr uint8_t RGB_COLOR_G = 0;
constexpr uint8_t RGB_COLOR_B = 40;   // gedimd blauw; WS2812 op 255 is fel
constexpr int DEFAULT_LED_ON_SEC  = 5;
constexpr int DEFAULT_LED_OFF_SEC = 5;
constexpr int MIN_LED_SEC         = 1;
constexpr int MAX_LED_SEC         = 3600;

// =================================================================
// 4. OTA-UPDATES VIA GITHUB RELEASES
// =================================================================
// De versie wordt bij een release door GitHub Actions meegegeven
// (-DFW_VERSION="v1.2.0"). Lokaal gebouwd = "dev": dan geen auto-update.
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
constexpr const char* FIRMWARE_VERSION = FW_VERSION;

// Vul hier je eigen (publieke!) repository in.
constexpr const char* GITHUB_OWNER = "jouw-github-naam";
constexpr const char* GITHUB_REPO  = "ProvisioningTest";

// Namen van de bestanden in elke release (zie .github/workflows/release.yml)
constexpr const char* OTA_FIRMWARE_ASSET = "firmware.bin";
constexpr const char* OTA_VERSION_ASSET  = "version.txt";

constexpr uint32_t OTA_CHECK_INTERVAL_MS = 6UL * 60 * 60 * 1000;  // elke 6 uur
```

### 6.3 `src/main.cpp`: het hoofdprogramma

```cpp
#include <Arduino.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "config.h"
#include "ota_github.h"

// Runtime-instellingen (geladen uit NVS, defaults uit config.h)
static int meetIntervalSec = DEFAULT_MEASURE_INTERVAL;
static int ledOnSec        = DEFAULT_LED_ON_SEC;
static int ledOffSec       = DEFAULT_LED_OFF_SEC;

// ---------------------------------------------------------------
// Onboard WS2812 RGB LED (data-LED: geen digitalWrite, maar neopixelWrite)
// ---------------------------------------------------------------
static void rgbOn()  { neopixelWrite(PIN_RGB_LED, RGB_COLOR_R, RGB_COLOR_G, RGB_COLOR_B); }
static void rgbOff() { neopixelWrite(PIN_RGB_LED, 0, 0, 0); }

// ---------------------------------------------------------------
// NVS helpers
// ---------------------------------------------------------------
static void loadSettings() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);  // read-only
  meetIntervalSec = prefs.getInt(NVS_KEY_INTERVAL, DEFAULT_MEASURE_INTERVAL);
  ledOnSec        = prefs.getInt(NVS_KEY_LED_ON, DEFAULT_LED_ON_SEC);
  ledOffSec       = prefs.getInt(NVS_KEY_LED_OFF, DEFAULT_LED_OFF_SEC);
  prefs.end();
  Serial.printf("Uit NVS: meetinterval %d s, LED aan %d s / uit %d s\n",
                meetIntervalSec, ledOnSec, ledOffSec);
}

// Schrijft alleen naar flash als de waarde geldig is én veranderd.
static void saveIntIfChanged(const char* key, int& huidig, int nieuw,
                             int minVal, int maxVal) {
  if (nieuw < minVal || nieuw > maxVal) {
    Serial.printf("Ongeldige waarde %d voor '%s' genegeerd.\n", nieuw, key);
    return;
  }
  if (nieuw == huidig) return;

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putInt(key, nieuw);
  prefs.end();
  huidig = nieuw;
  Serial.printf("'%s' opgeslagen in NVS: %d\n", key, nieuw);
}

// ---------------------------------------------------------------
// Captive portal (alleen op verzoek van de gebruiker)
// ---------------------------------------------------------------
static void runConfigPortal() {
  Serial.println("Access Point wordt geactiveerd...");
  digitalWrite(PIN_STATUS_LED, HIGH);
  rgbOff();  // knipperen pauzeert tijdens setup

  WiFiManager wm;

  char intervalStr[12], ledOnStr[12], ledOffStr[12];
  snprintf(intervalStr, sizeof(intervalStr), "%d", meetIntervalSec);
  snprintf(ledOnStr, sizeof(ledOnStr), "%d", ledOnSec);
  snprintf(ledOffStr, sizeof(ledOffStr), "%d", ledOffSec);

  WiFiManagerParameter pInterval(NVS_KEY_INTERVAL, "Meetinterval (sec)",
                                 intervalStr, 6, "type='number' min='1'");
  WiFiManagerParameter pLedOn(NVS_KEY_LED_ON, "RGB LED aan (sec)",
                              ledOnStr, 5, "type='number' min='1'");
  WiFiManagerParameter pLedOff(NVS_KEY_LED_OFF, "RGB LED uit (sec)",
                               ledOffStr, 5, "type='number' min='1'");
  wm.addParameter(&pInterval);
  wm.addParameter(&pLedOn);
  wm.addParameter(&pLedOff);

  wm.setConfigPortalTimeout(AP_TIMEOUT_SEC);
  wm.setBreakAfterConfig(true);  // ook terugkeren als Wi-Fi-verbinding faalt

  bool verbonden = wm.startConfigPortal(AP_SSID_NAME, AP_PASSWORD);

  // Eerst parameters verwerken, ongeacht of Wi-Fi lukte
  saveIntIfChanged(NVS_KEY_INTERVAL, meetIntervalSec, atoi(pInterval.getValue()),
                   MIN_MEASURE_INTERVAL, MAX_MEASURE_INTERVAL);
  saveIntIfChanged(NVS_KEY_LED_ON, ledOnSec, atoi(pLedOn.getValue()),
                   MIN_LED_SEC, MAX_LED_SEC);
  saveIntIfChanged(NVS_KEY_LED_OFF, ledOffSec, atoi(pLedOff.getValue()),
                   MIN_LED_SEC, MAX_LED_SEC);
  digitalWrite(PIN_STATUS_LED, LOW);

  if (!verbonden) {
    Serial.println("Portal gesloten zonder Wi-Fi verbinding. Herstarten...");
    delay(1000);
    ESP.restart();
  }
}

// Wacht kort na reset op de CONFIG-knop. GPIO 27 is geen strapping-pin,
// dus de knop mag ook al ingedrukt zijn tijdens de reset.
static bool configRequestedAtBoot() {
  Serial.printf("Druk binnen %lu s op de CONFIG-knop voor setup-modus...\n",
                (unsigned long)(CONFIG_WINDOW_MS / 1000));
  uint32_t start = millis();
  while (millis() - start < CONFIG_WINDOW_MS) {
    digitalWrite(PIN_STATUS_LED, (millis() / 100) % 2);  // snel knipperen
    if (digitalRead(PIN_BUTTON_CONFIG) == LOW) {
      digitalWrite(PIN_STATUS_LED, LOW);
      return true;
    }
    delay(10);
  }
  digitalWrite(PIN_STATUS_LED, LOW);
  return false;
}

// Niet-blokkerend knipperen: aan ledOnSec, uit ledOffSec.
static void updateBlinkLed() {
  static bool aan = false;
  static uint32_t sinds = 0;
  uint32_t duur = (uint32_t)(aan ? ledOnSec : ledOffSec) * 1000UL;
  if (millis() - sinds >= duur) {
    aan = !aan;
    sinds = millis();
    if (aan) rgbOn(); else rgbOff();
  }
}

// ---------------------------------------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  pinMode(PIN_BUTTON_CONFIG, INPUT_PULLUP);
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);
  rgbOff();
  delay(500);
  Serial.printf("\n=== ESP32 Systeem Opstarten (firmware %s) ===\n", FIRMWARE_VERSION);

  loadSettings();

  if (configRequestedAtBoot()) {
    runConfigPortal();
  } else {
    Serial.println("Normale opstart. Verbinden met Wi-Fi...");
    WiFiManager wm;
    wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT);
    wm.setEnableConfigPortal(false);  // NOOIT automatisch een AP openen
    if (!wm.autoConnect()) {
      Serial.println("Wi-Fi niet bereikbaar. Offline modus.");
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi verbonden! IP: ");
    Serial.println(WiFi.localIP());
    otaCheckForUpdate();  // nieuwe release op GitHub?
  }
}

void loop() {
  // Lang indrukken van de CONFIG-knop tijdens bedrijf -> setup-modus
  static uint32_t knopSinds = 0;
  if (digitalRead(PIN_BUTTON_CONFIG) == LOW) {
    if (knopSinds == 0) knopSinds = millis();
    else if (millis() - knopSinds >= CONFIG_LONGPRESS_MS) {
      knopSinds = 0;
      runConfigPortal();
    }
  } else {
    knopSinds = 0;
  }

  updateBlinkLed();

  // Periodiek kijken of er een nieuwe release op GitHub staat
  static uint32_t vorigeOtaCheck = millis();
  if (millis() - vorigeOtaCheck >= OTA_CHECK_INTERVAL_MS) {
    vorigeOtaCheck = millis();
    otaCheckForUpdate();
  }

  static uint32_t vorigeTijd = 0;
  if (millis() - vorigeTijd >= (uint32_t)meetIntervalSec * 1000UL) {
    vorigeTijd = millis();
    int rawValue = analogRead(PIN_SENSOR_ANALOG);
    Serial.printf("[%lu ms] GPIO %u: %d | volgende meting over %d s\n",
                  (unsigned long)millis(), PIN_SENSOR_ANALOG, rawValue,
                  meetIntervalSec);
  }
}
```

### 6.4 `include/ota_github.h` en `src/ota_github.cpp`: OTA via GitHub (uitleg in hoofdstuk 8)

```cpp
#pragma once

// Controleert de nieuwste GitHub Release en installeert die als ze verschilt
// van de huidige firmwareversie. Blokkeert tijdens de download en herstart
// de ESP32 na een geslaagde update.
void otaCheckForUpdate();
```

```cpp
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
```

### 6.5 `include/github_ca.h`: root-certificaten (ingekort)

```cpp
#pragma once

// Root-certificaten waarmee de ESP32 de HTTPS-verbinding met GitHub controleert.
//  - github.com                          -> Sectigo, ondertekend door USERTrust ECC
//  - release-assets.githubusercontent.com -> Let's Encrypt, ondertekend door ISRG Root X1
// Gecontroleerd op 2026-11-13. Wisselt GitHub van CA, dan faalt OTA met een
// TLS-fout en moet je deze lijst bijwerken.

static const char GITHUB_ROOT_CA[] = R"PEM(
-----BEGIN CERTIFICATE-----
MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDEL
... (4 certificaten, zie het bestand zelf) ...
-----END CERTIFICATE-----
)PEM";
```

### 6.6 `.github/workflows/release.yml`: automatisch bouwen en publiceren

```yaml
# Bouwt de firmware en publiceert een GitHub Release zodra je een tag
# zoals v1.0.0 pusht:   git tag v1.0.0 && git push origin v1.0.0
name: Release firmware

on:
  push:
    tags: ["v*"]

permissions:
  contents: write   # nodig om een release aan te maken

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - uses: actions/setup-python@v5
        with:
          python-version: "3.11"

      - name: PlatformIO installeren
        run: pip install platformio

      - name: Firmware bouwen met versienummer uit de tag
        env:
          PLATFORMIO_BUILD_FLAGS: -DFW_VERSION=\"${{ github.ref_name }}\"
        run: pio run -e dfrobot_firebeetle2_esp32e

      - name: Release-bestanden klaarzetten
        run: |
          cp .pio/build/dfrobot_firebeetle2_esp32e/firmware.bin firmware.bin
          echo "${{ github.ref_name }}" > version.txt

      - name: GitHub Release publiceren
        uses: softprops/action-gh-release@v2
        with:
          files: |
            firmware.bin
            version.txt
```

---

## 7. De code uitgelegd

### 7.1 Preferences initialiseren met defaults uit `config.h`

```cpp
meetIntervalSec = prefs.getInt(NVS_KEY_INTERVAL, DEFAULT_MEASURE_INTERVAL);
```

`getInt()` zoekt de sleutel `"interval"` in NVS. Bestaat die nog niet (een nieuw board), dan geeft `getInt()` de tweede parameter terug: `DEFAULT_MEASURE_INTERVAL` (30) uit `config.h`. Pas wanneer de gebruiker in het portal een nieuwe waarde opslaat, schrijft `putInt()` die naar flash.

We openen de namespace telkens **kort**: `begin(..., true)` (read-only) om te lezen, `begin(..., false)` om te schrijven, en daarna meteen `end()`.

### 7.2 Het knopvenster na het opstarten

`configRequestedAtBoot()` laat de status-LED 3 seconden snel knipperen. Wordt de knop in die tijd ingedrukt (of al vastgehouden tijdens de reset), dan start het portal. Anders volgt een normale opstart.

Tijdens normaal bedrijf opent het portal ook als je de knop **5 seconden** ingedrukt houdt (zie `loop()`).

### 7.3 Custom parameters in het portal

```cpp
WiFiManagerParameter pInterval(NVS_KEY_INTERVAL, "Meetinterval (sec)",
                               intervalStr, 6, "type='number' min='1'");
wm.addParameter(&pInterval);
```

| Argument | Betekenis |
|---|---|
| `NVS_KEY_INTERVAL` | HTML-id van het veld |
| `"Meetinterval (sec)"` | Label op de pagina |
| `intervalStr` | Huidige waarde (als tekst) die al ingevuld staat |
| `6` | Maximale lengte |
| `"type='number' min='1'"` | Extra HTML: de smartphone toont een numeriek toetsenbord |

Andere ideeën voor custom velden: MQTT-broker-adres, API-key, apparaatnaam (`Kas-Zone-3`), alarmdrempels.

### 7.4 Tekst ↔ getal: `atoi()` en `snprintf()`

Een webformulier levert **altijd tekst** aan. Om ermee te rekenen moet je die omzetten:

| Functie | Invoer | Uitvoer | Gebruik hier |
|---|---|---|---|
| `atoi(tekst)` | `"120"` | `120` (int) | Ingevulde waarde uit het portal omzetten naar een getal |
| `atof(tekst)` | `"22.5"` | `22.5` (float) | Bv. een temperatuurdrempel |
| `snprintf(buf, size, "%d", getal)` | `30` | `"30"` | Huidige waarde klaarzetten in het HTML-veld |

We gebruiken `snprintf()` in plaats van `itoa()`: het is standaard C/C++ en controleert de buffergrootte.

> **Let op:** `atoi("abc")` geeft `0` terug, zonder foutmelding. Daarom controleert `saveIntIfChanged()` altijd of de waarde binnen een geldig bereik valt.

### 7.5 Eerst opslaan, dan pas herstarten

```cpp
bool verbonden = wm.startConfigPortal(AP_SSID_NAME, AP_PASSWORD);
saveIntIfChanged(...);   // altijd eerst de parameters bewaren
if (!verbonden) ESP.restart();
```

Typt de gebruiker het Wi-Fi-wachtwoord verkeerd, dan zijn de andere instellingen toch bewaard. `setBreakAfterConfig(true)` zorgt dat `startConfigPortal()` ook in dat geval terugkeert.

### 7.6 Niet-blokkerend knipperen met `millis()`

```cpp
uint32_t duur = (uint32_t)(aan ? ledOnSec : ledOffSec) * 1000UL;
if (millis() - sinds >= duur) { aan = !aan; sinds = millis(); /* LED wisselen */ }
```

Met `delay(5000)` zou je hele programma 5 seconden stilstaan: geen metingen, geen knopdetectie. Met `millis()` controleer je enkel *of het al tijd is*, en loopt de rest van `loop()` gewoon door. `millis() - sinds` blijft ook correct wanneer `millis()` na ±49 dagen terug naar 0 springt.

### 7.7 De RGB LED: `neopixelWrite()`

De onboard LED op GPIO 5 is een **WS2812**: een "slimme" LED met een eigen chipje dat een digitaal datasignaal verwacht met de kleurwaarden (R, G, B). Met `digitalWrite(5, HIGH)` gebeurt er dus niets. Het ESP32-framework heeft een ingebouwde functie, dus je hebt geen extra bibliotheek nodig:

```cpp
neopixelWrite(PIN_RGB_LED, 0, 0, 40);  // gedimd blauw
neopixelWrite(PIN_RGB_LED, 0, 0, 0);   // uit
```

> **Tip:** WS2812-LEDs zijn op 255 erg fel. Waarden rond 20–60 zijn prima voor een status-LED.

---

## 8. OTA-updates via GitHub Releases

Met het captive portal pas je **instellingen** aan zonder kabel. Met **OTA** (*Over-The-Air*) doe je hetzelfde voor de **firmware zelf**. In dit project gebruiken we **GitHub Releases** als updateserver:

- Je hebt geen eigen server nodig.
- GitHub Actions bouwt de firmware automatisch.
- Elke versie krijgt een duidelijk versienummer.

### 8.1 Hoe het werkt

1. Je pusht een **tag** naar GitHub, bijvoorbeeld `v1.1.0`.
2. **GitHub Actions** ([.github/workflows/release.yml](.github/workflows/release.yml)) bouwt de firmware met dat versienummer erin.
3. De workflow maakt een **Release** met twee bestanden:
   - `firmware.bin`: de nieuwe firmware;
   - `version.txt`: één regel met de versie (`v1.1.0`).
4. De ESP32 downloadt `version.txt` **bij elke opstart** (na de Wi-Fi-verbinding) en daarna **elke 6 uur**.
5. Verschilt die versie van de eigen versie (`FIRMWARE_VERSION`)? Dan downloadt de ESP32 `firmware.bin`, schrijft die naar de **tweede app-partitie** en herstart.

Het vaste adres `https://github.com/<owner>/<repo>/releases/latest/download/version.txt` verwijst altijd naar de **nieuwste** release. GitHub stuurt het door (HTTP 302) naar de echte downloadserver. Daarom staat `setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS)` in de code.

### 8.2 Twee app-partities: waarom een mislukte update geen kapot apparaat geeft

Het flashgeheugen van de ESP32 heeft (met de standaard partitietabel) **twee app-partities** van elk 1,25 MB: `app0` en `app1`.

- De nieuwe firmware wordt naar de partitie geschreven die **niet** in gebruik is.
- Pas als de download volledig en correct is, wisselt de bootloader van partitie.
- Valt de Wi-Fi of de stroom weg tijdens de download, dan start de oude firmware gewoon opnieuw.

Je firmware moet dus kleiner zijn dan 1,25 MB. Deze firmware is ongeveer 880 KB.

> **Ook na een OTA-update blijven je instellingen bewaard:** de NVS-partitie wordt niet aangeraakt (zie hoofdstuk 9).

### 8.3 Het versienummer

In [config.h](include/config.h):

```cpp
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
constexpr const char* FIRMWARE_VERSION = FW_VERSION;
```

- **Bouw je lokaal** (`pio run`), dan is de versie `"dev"`. Een dev-build **zoekt niet naar updates**. Anders overschrijft de ESP32 je testcode meteen met de laatste release.
- **GitHub Actions** geeft de tag mee als build-flag, via de omgevingsvariabele `PLATFORMIO_BUILD_FLAGS`:

  ```yaml
  PLATFORMIO_BUILD_FLAGS: -DFW_VERSION=\"${{ github.ref_name }}\"
  ```

  Daardoor wordt `FW_VERSION` bijvoorbeeld `"v1.1.0"`.

### 8.4 HTTPS: waarom de certificaten in `github_ca.h` staan

Firmware downloaden over een onbeveiligde verbinding is gevaarlijk: wie tussen je ESP32 en het internet zit, kan eigen code laten installeren. Veel voorbeelden online gebruiken `client.setInsecure()`. Dat schakelt de controle uit, dus **doe dat niet**.

Wij geven de ESP32 de **root-certificaten** mee die GitHub gebruikt ([include/github_ca.h](include/github_ca.h)):

| Server | Certificaat van | Root |
|---|---|---|
| `github.com` | Sectigo | USERTrust ECC (en RSA als reserve) |
| `release-assets.githubusercontent.com` | Let's Encrypt | ISRG Root X1 (en X2 als reserve) |

Twee aandachtspunten:

- **Klok:** certificaten hebben een geldigheidsperiode. Daarom zet `syncClock()` eerst de klok gelijk via NTP (`pool.ntp.org`).
- **Andere CA bij GitHub:** stapt GitHub ooit over naar een andere certificaatautoriteit, dan faalt de update met een TLS-fout. Dan moet je `github_ca.h` bijwerken en de nieuwe firmware **één keer via USB** flashen.

### 8.5 Stap voor stap instellen

**1. Maak een publieke GitHub-repository** en vul in [config.h](include/config.h) je gegevens in:

```cpp
constexpr const char* GITHUB_OWNER = "jouw-github-naam";
constexpr const char* GITHUB_REPO  = "ProvisioningTest";
```

**2. Zet het project op GitHub** (de map `.pio` staat al in `.gitignore`):

```bash
git init
git add .
git commit -m "Eerste versie"
git branch -M main
git remote add origin https://github.com/jouw-github-naam/ProvisioningTest.git
git push -u origin main
```

> **Let op:** een release wordt gebouwd uit de code die **op GitHub** staat, niet uit je lokale bestanden. Vul `GITHUB_OWNER` in vóór je commit. Heb je al gepusht met `"jouw-github-naam"`? Commit en push dan eerst je aangepaste `config.h`, en maak pas daarna de tag. Controleer met `git status` dat er niets meer openstaat.

**3. Maak de eerste release:**

```bash
git tag v1.0.0
git push origin v1.0.0
```

Volg de build onder het tabblad **Actions**. Na ongeveer 2 minuten staat er onder **Releases** een `v1.0.0` met `firmware.bin` en `version.txt`.

**4. Flash één keer via USB een build met versie `v1.0.0`.** Een dev-build zoekt immers niet naar updates. In de PowerShell-terminal van VS Code:

```powershell
$env:PLATFORMIO_BUILD_FLAGS='-DFW_VERSION=\"v1.0.0\"'; pio run -t upload; Remove-Item Env:PLATFORMIO_BUILD_FLAGS
```

**5. Vanaf nu gaan updates draadloos:** pas de code aan, commit, en push een nieuwe tag (`v1.0.1`, `v1.1.0`, …).

### 8.6 Beperkingen en veiligheid

- **De repository moet publiek zijn.** Bij een private repository heeft de ESP32 een GitHub-token nodig, en dat token kan iedereen uit de flash lezen. Doe dat dus niet.
- **Publiek betekent dat iedereen je code ziet**, ook `AP_PASSWORD` in `config.h`. Zet daar geen echte geheimen in.
- **Wie push-rechten heeft op de repository, bepaalt wat er op al je apparaten draait.** Beveilig je GitHub-account met tweestapsverificatie.
- **De ESP32 vergelijkt alleen of de versie *anders* is**, niet of ze *nieuwer* is. Verwijder je de nieuwste release, dan gaan je apparaten terug naar de vorige versie. Dat is handig als een update kapot blijkt.
- **De download blokkeert ±10–30 seconden.** In die tijd brandt de status-LED en staan metingen en knipperen stil.

---

## 9. Levensduur van het flashgeheugen

Flash kan maar een beperkt aantal keer overschreven worden (orde van grootte 100.000 cycli per blok). Lezen is onbeperkt; **schrijven** veroorzaakt slijtage. NVS spreidt de schrijfacties wel over het geheugen (wear levelling), maar blijf zuinig:

- **Roep nooit `putInt()` / `putString()` onvoorwaardelijk aan in `loop()`.** Duizenden keren per seconde schrijven verslijt het flash razendsnel.
- **Schrijf alleen als de waarde echt veranderd is.** Dat doet `saveIntIfChanged()`.
- **Sluit de namespace met `end()`** zodra je klaar bent.

### Wat blijft bewaard?

| Gebeurtenis | RAM | Preferences (NVS) |
|---|---|---|
| Stroom eraf (USB/accu los) | Gewist | **Bewaard** |
| RESET-knop / `ESP.restart()` | Gewist | **Bewaard** |
| Ontwaken uit deep sleep | Gewist (behalve RTC-RAM) | **Bewaard** |
| Nieuwe firmware uploaden (USB of OTA) | Gewist | **Bewaard** |
| `preferences.clear()` | Onveranderd | Gewist (die namespace) |
| Volledige flash-erase (`pio run -t erase`) | Gewist | Gewist |

---

## 10. Testen

1. **Uploaden:** `pio run -t upload`
2. **Seriële monitor:** `pio device monitor` (115200 baud)
3. **Eerste opstart:** je ziet `meetinterval 30 s, LED aan 5 s / uit 5 s` (de defaults uit `config.h`) en de RGB LED knippert blauw.
4. **Portal openen:** druk op RESET en daarna, terwijl de blauwe status-LED snel knippert, op de knop op GPIO 27. De status-LED blijft branden.
5. **Verbinden:** verbind je smartphone met `FireBeetle-Setup` (wachtwoord `supergeheim`). De pagina opent vanzelf; zo niet, surf dan naar `http://192.168.4.1`.
6. **Instellen:** kies *Configure WiFi*, vul je netwerk in en zet bijvoorbeeld *Meetinterval* op `5`, *RGB LED aan* op `1` en *uit* op `3`. Klik op *Save*.
7. **Controleren:** metingen komen nu elke 5 s, de LED knippert 1 s aan / 3 s uit.
8. **Bewaard?** Druk op RESET *zonder* de knop: de nieuwe waarden blijven en de ESP32 verbindt vanzelf met je Wi-Fi.

### OTA testen

9. Flash `v1.0.0` via USB (hoofdstuk 8.5, stap 4). In de monitor zie je `firmware v1.0.0` en `[OTA] Firmware is up-to-date (v1.0.0).`
10. Verander iets zichtbaars, bijvoorbeeld `RGB_COLOR_G = 40` in `config.h` (de LED wordt cyaan). Commit, push en tag `v1.0.1`.
11. Wacht tot de release klaar is en druk op RESET. Je ziet:

    ```text
    [OTA] Nieuwe versie v1.0.1 gevonden (huidig: v1.0.0). Downloaden...
    [OTA] 0%
    ...
    [OTA] 100%
    ```

    Daarna herstart de ESP32 met `firmware v1.0.1`, en je meetinterval en knippertijden zijn nog steeds bewaard.

> **Portal opent nooit, of juist altijd?** Dan is de knop op jouw board misschien actief HIGH. Vervang dan `INPUT_PULLUP` door `INPUT_PULLDOWN` en `== LOW` door `== HIGH`.

---

## 11. Veelgemaakte fouten

Deze fouten komen vaak voor in voorbeeldcode online:

| Fout | Gevolg | Correcte aanpak |
|---|---|---|
| Verkeerde LED-pin aannemen (bv. GPIO 25) | LED doet niets | Pin-definities van je board nakijken (`LED_BUILTIN` = GPIO 2) |
| Knop op GPIO 0 ingedrukt houden tijdens reset | ESP32 start in download-modus | Een niet-strapping-pin gebruiken (GPIO 27) of de knop pas *na* de reset uitlezen |
| `autoConnect()` met standaardinstellingen | Open setup-netwerk zonder wachtwoord bij elke Wi-Fi-storing | `setEnableConfigPortal(false)` |
| Herstarten vóór het opslaan van parameters | Ingevulde instellingen gaan verloren | Eerst opslaan, dan pas `ESP.restart()` |
| Alleen controleren op `> 0` | Te grote waarden geven een overflow in `* 1000` | Minimum én maximum controleren |
| Namespace open laten tijdens het portal | Onnodig RAM-gebruik | Kort openen, meteen `end()` |
| `digitalWrite()` op een WS2812 | LED blijft uit | `neopixelWrite()` |
| `String` aan elkaar plakken in `loop()` | Op termijn versnipperd werkgeheugen | `Serial.printf()` |
| `client.setInsecure()` bij OTA | Iedereen tussen ESP32 en internet kan eigen firmware installeren | Root-certificaten meegeven (`github_ca.h`) |
| OTA-adres zonder redirects volgen | HTTP 302 in plaats van de firmware | `setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS)` |
| Private repository voor OTA | HTTP 404, of een token dat iedereen uit de flash kan lezen | Publieke repository, zonder geheimen in de code |
| Lokale dev-build flashen en op een update wachten | Er gebeurt niets: een dev-build zoekt niet naar updates | Eén keer een build met versienummer flashen (8.5, stap 4) |

---

## 12. Vergelijking van provisioningmethodes

| Eigenschap | Compile-time (`config.h`) | Captive portal + Preferences | BLE provisioning (app) |
|---|---|---|---|
| Instelbare parameters | Geen (alleen door te flashen) | Wi-Fi + zoveel custom velden als je wil | Ja, via app-payloads |
| Beveiliging | n.v.t. | Veilig als het AP alleen via een knop start | Veilig via Bluetooth-koppeling |
| Software op smartphone | Geen | Geen (gewone browser) | Specifieke Android/iOS-app nodig |
| Geheugengebruik | Minimaal | Gemiddeld (web- en DNS-server) | Hoog (Bluetooth-stack) |

---

## 13. Samengevat: aanbevolen aanpak

1. **Vaste hardware-instellingen** (pinnen, baudrate, sleutelnamen) bewaar je als `constexpr` in `config.h`.
2. **Standaardwaarden** in `config.h` gebruik je als fallback bij `getInt()` / `getString()`.
3. **Het captive portal** start je alleen bewust, met een fysieke knop, en altijd met een wachtwoord.
4. **Runtime-instellingen** sla je op met `Preferences.h`, alleen als ze geldig én veranderd zijn.
5. **Timing** doe je met `millis()` in plaats van `delay()`, zodat je programma reageert op knoppen.
6. **Firmware-updates** lever je via GitHub Releases, over HTTPS met gecontroleerde certificaten. Je instellingen in NVS blijven daarbij gewoon bewaard.
