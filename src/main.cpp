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
