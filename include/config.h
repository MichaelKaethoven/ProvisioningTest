#pragma once

#include <Arduino.h>

// =================================================================
// 1. HARDWARE CONSTANTEN (compile-time)
// =================================================================
constexpr uint8_t PIN_BUTTON_CONFIG =
    27; // Onboard user-knop (GPIO 27, actief LOW)
constexpr uint8_t PIN_STATUS_LED =
    LED_BUILTIN; // Onboard LED (GPIO 2 op FireBeetle 2 ESP32-E)
constexpr uint8_t PIN_SENSOR_ANALOG = A0; // A0 = GPIO 36
constexpr uint8_t PIN_RGB_LED = 5;        // Onboard WS2812 RGB LED (GPIO 5)

// =================================================================
// 2. COMMUNICATIE / PORTAL CONSTANTEN
// =================================================================
constexpr uint32_t SERIAL_BAUD_RATE = 115200;
constexpr uint16_t AP_TIMEOUT_SEC = 180;      // Max 3 minuten in setup-modus
constexpr uint16_t WIFI_CONNECT_TIMEOUT = 10; // Seconden
constexpr const char *AP_SSID_NAME = "FireBeetle-Setup";
constexpr const char *AP_PASSWORD = "supergeheim"; // min. 8 tekens

// Na reset heb je dit venster om de CONFIG-knop in te drukken (of je houdt hem
// al vast).
constexpr uint32_t CONFIG_WINDOW_MS = 3000;
// Tijdens normaal bedrijf: knop zo lang ingedrukt houden -> portal.
constexpr uint32_t CONFIG_LONGPRESS_MS = 5000;

// =================================================================
// 3. PREFERENCES NVS SLEUTELS EN DEFAULTS
// Namespace en key: elk MAXIMAAL 15 tekens!
// =================================================================
constexpr const char *NVS_NAMESPACE = "app-config";
constexpr const char *NVS_KEY_INTERVAL = "interval";
constexpr const char *NVS_KEY_LED_ON = "led_on";
constexpr const char *NVS_KEY_LED_OFF = "led_off";

constexpr int DEFAULT_MEASURE_INTERVAL = 30; // seconden
constexpr int MIN_MEASURE_INTERVAL = 1;
constexpr int MAX_MEASURE_INTERVAL = 86400; // 1 dag

// RGB LED op GPIO 5: kleur (0-255 per kanaal) en knippertijden (seconden)
constexpr uint8_t RGB_COLOR_R = 0;
constexpr uint8_t RGB_COLOR_G = 0;
constexpr uint8_t RGB_COLOR_B = 40; // gedimd blauw; WS2812 op 255 is fel
constexpr int DEFAULT_LED_ON_SEC = 5;
constexpr int DEFAULT_LED_OFF_SEC = 5;
constexpr int MIN_LED_SEC = 1;
constexpr int MAX_LED_SEC = 3600;

// =================================================================
// 4. OTA-UPDATES VIA GITHUB RELEASES
// ===================Z==============================================
// De versie wordt bij een release door GitHub Actions meegegeven
// (-DFW_VERSION="v1.2.0"). Lokaal gebouwd = "dev": dan geen auto-update.
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
constexpr const char *FIRMWARE_VERSION = FW_VERSION;

// Vul hier je eigen (publieke!) repository in.
constexpr const char *GITHUB_OWNER = "MichaelKaethoven";
constexpr const char *GITHUB_REPO = "ProvisioningTest";

// Namen van de bestanden in elke release (zie .github/workflows/release.yml)
constexpr const char *OTA_FIRMWARE_ASSET = "firmware.bin";
constexpr const char *OTA_VERSION_ASSET = "version.txt";

constexpr uint32_t OTA_CHECK_INTERVAL_MS = 6UL * 60 * 60 * 1000; // elke 6 uur
