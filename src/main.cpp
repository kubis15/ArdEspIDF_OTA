#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <time.h>

#include "version_check.h"
#include "ota_client.h"
#include "rollback.h"

// Declare bootMillis as external so it can be referenced across modules
extern uint32_t bootMillis;
// Global flag for manual OTA update requests (set by /OTAnow)
extern volatile bool pendingOTAUpdate;

// ============================================================
// Configuration
// ============================================================

// Boot-time budgets (ms)
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;   // then fall back to AP
static const uint32_t NTP_SYNC_TIMEOUT_MS     = 10000;   // best-effort only
static const uint32_t AP_STA_RETRY_MS         = 300000;  // retry STA every 5 min from AP

// Supplied via build_flags from secrets.ini; absent -> empty -> the board
// boots straight into the recovery AP until provisioned over /save.
#ifndef WIFI_SSID_DEFAULT
#define WIFI_SSID_DEFAULT ""
#endif
#ifndef WIFI_PASS_DEFAULT
#define WIFI_PASS_DEFAULT ""
#endif
#ifndef AP_PASSWORD
#define AP_PASSWORD "recover-me"     // WPA2 needs >= 8 chars; change this
#endif

// YD-ESP32-S3 onboard WS2812 data pin
#define LED_PIN 48

static Preferences prefs;
static String   wifiSsid;
static String   wifiPass;
static bool     apMode   = false;
static volatile uint32_t rebootAt = 0;   // 0 = none pending

// ============================================================
// Helpers
// ============================================================

static void ledStatus(uint8_t r, uint8_t g, uint8_t b) {
    rgbLedWrite(LED_PIN, r, g, b);
}

static void saveCredentials(const String &ssid, const String &pass) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
}

static void loadCredentials() {
    prefs.begin("wifi", true);               // read-only
    wifiSsid = prefs.getString("ssid", "");
    wifiPass = prefs.getString("pass", "");
    prefs.end();

    // First boot of a board flashed with compiled-in credentials: seed NVS so
    // later credential-free builds keep working. This is what lets us stop
    // baking secrets into the published binary without stranding any board.
    if (wifiSsid.isEmpty() && strlen(WIFI_SSID_DEFAULT) > 0) {
        wifiSsid = WIFI_SSID_DEFAULT;
        wifiPass = WIFI_PASS_DEFAULT;
        saveCredentials(wifiSsid, wifiPass);
        Serial.println("Seeded NVS credentials from build-time defaults.");
    }
}

static bool connectSTA(uint32_t timeoutMs) {
    if (wifiSsid.isEmpty()) {
        Serial.println("No WiFi credentials provisioned.");
        return false;
    }

    Serial.printf("Connecting to \"%s\"", wifiSsid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());

    const uint32_t deadline = millis() + timeoutMs;
    bool blink = false;

    while (WiFi.status() != WL_CONNECTED) {
        // Signed-difference compare is rollover-safe across the ~49-day millis() wrap.
        if ((int32_t)(millis() - deadline) >= 0) {
            Serial.printf("\nWiFi connect timed out after %u ms (status %d)\n",
                          timeoutMs, WiFi.status());
            return false;
        }
        blink = !blink;
        ledStatus(0, 0, blink ? 50 : 0);          // pulsing blue = connecting
        Serial.print(".");
        delay(500);
    }

    Serial.printf("\nWiFi connected. IP %s  MAC %s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.macAddress().c_str());
    ledStatus(0, 50, 0);                          // green = online
    return true;
}

// Best-effort. Never fatal: nothing in the OTA path needs a correct clock
// (mbedTLS cert date checks are off in the default IDF config), the
// timestamps are for logging.
static bool syncTime(uint32_t timeoutMs) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Synchronizing time");

    const uint32_t deadline = millis() + timeoutMs;
    time_t now = time(nullptr);

    // 1700000000 unix equivalent of 2023-11-14 22:13:20 UTC

    while (now < 1700000000) {
        if ((int32_t)(millis() - deadline) >= 0) {
            Serial.println("\nNTP sync timed out - continuing with unset clock.");
            return false;
        }
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }

    Serial.printf("\nTime synced: %s", ctime(&now));
    return true;
}

// ============================================================
// Recovery AP
// ============================================================

static AsyncWebServer apServer(80);

static void startRecoveryAP() {
    apMode = true;

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "ESP32-OTA-Recovery-%02X%02X", mac[4], mac[5]);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, AP_PASSWORD);

    Serial.printf("\n*** RECOVERY AP ***\n  SSID: %s\n  Pass: %s\n  URL:  http://%s/\n",
                  ssid, AP_PASSWORD, WiFi.softAPIP().toString().c_str());
    ledStatus(50, 25, 0);                         // amber = recovery

    // Register handlers and start the server only once. This function is
    // re-entered from the loop() retry path, and re-registering handlers on
    // the live server would stack up duplicates.
    static bool serverStarted = false;
    if (serverStarted) return;
    serverStarted = true;

    apServer.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/html",
            "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
            "<h2>OTA Recovery</h2>"
            "<p>Firmware v" CURRENT_VERSION "</p>"
            "<form method=POST action=/save>"
            "<p>SSID<br><input name=ssid maxlength=32 required>"
            "<p>Password<br><input name=pass type=password maxlength=63>"
            "<p><button>Save &amp; reboot</button></form>");
    });

    // curl -X POST http://192.168.4.1/save \
    //     -d "ssid=MyWiFiName" \
    //     -d "pass=MyWiFiPassword"

    apServer.on("/save", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (!req->hasParam("ssid", true)) {
            req->send(400, "text/plain", "missing ssid");
            return;
        }
        String ssid = req->getParam("ssid", true)->value();
        String pass = req->hasParam("pass", true)
                        ? req->getParam("pass", true)->value() : String();

        saveCredentials(ssid, pass);
        Serial.printf("Saved credentials for \"%s\" - rebooting\n", ssid.c_str());
        req->send(200, "text/html", "<h3>Saved. Rebooting...</h3>");

        // Do NOT esp_restart() here - this runs on the AsyncTCP task and the
        // response would never flush. Hand the reboot to loop().
        rebootAt = millis() + 1000;
    });

    apServer.begin();
}

// ============================================================
// Setup
// ============================================================

void setup() {
    // Capture the boot timestamp early in setup()
    bootMillis = millis();

    Serial.begin(115200);
    delay(1000);

    Serial.printf("\n--- Starting Firmware v%s ---\n", CURRENT_VERSION);
    Serial.printf("Boot reason: %d\n", esp_reset_reason());
    Serial.printf("Flash: %u MB   PSRAM: %u MB\n",
                  ESP.getFlashChipSize() / (1024 * 1024),
                  ESP.getPsramSize() / (1024 * 1024));

    // GPIO override + boot-attempt budget; may restart into the other slot
    rollbackCheckOnBoot();

    loadCredentials();

    if (!connectSTA(WIFI_CONNECT_TIMEOUT_MS)) {
        rollbackOnTrialConnectFailed();  // trial image that can't connect -> revert
        startRecoveryAP();
        return;                          // loop() handles retry + reboot
    }

    syncTime(NTP_SYNC_TIMEOUT_MS);       // best-effort, never fatal

    // Start the AsyncWebServer that serves /version and /OTAnow
    startVersionServer();

    // Run OTA update check
    esp_err_t ota_result = check_for_ota_update();

    if (ota_result != ESP_OK)
    {
        Serial.printf(
            "OTA check failed: %s\n",
            esp_err_to_name(ota_result)
        );
    }
    else
    {
        // Self-test passed: WiFi up + manifest fetched over TLS.
        // (An actual update never returns - it restarts.)
        rollbackMarkHealthy();
    }
}

// ============================================================
// Loop
// ============================================================

void loop() {
    // Deferred reboot from the /save handler (never restart on the AsyncTCP task)
    if (rebootAt && (int32_t)(millis() - rebootAt) >= 0) {
        Serial.println("Rebooting to apply new credentials...");
        Serial.flush();
        esp_restart();
    }

    // Recovery mode: retry saved credentials periodically. No OTA work here -
    // the AP has no uplink, it only serves / and /save on the local network.
    if (apMode) {
        static uint32_t nextRetry = AP_STA_RETRY_MS;
        if ((int32_t)(millis() - nextRetry) >= 0) {
            nextRetry = millis() + AP_STA_RETRY_MS;
            Serial.println("Retrying saved credentials from recovery mode...");
            loadCredentials();
            WiFi.softAPdisconnect(true);
            if (connectSTA(15000)) {
                esp_restart();           // simplest: reboot into the normal path
            }
            startRecoveryAP();           // still no luck - back to recovery
        }
        ledStatus(50, 25, 0);
        delay(200);
        return;
    }

    // Heartbeat log, throttled so it doesn't flood the console
    static uint32_t nextHeartbeat = 0;
    if ((int32_t)(millis() - nextHeartbeat) >= 0) {
        nextHeartbeat = millis() + 2000;
        Serial.printf("Looping... %s\n", pendingOTAUpdate ? "true" : "false");
    }

    // Process scheduled or flagged OTA requests
    // Later to convert to a FreeRTOS task, but for now, just check the flag in loop()
    if (pendingOTAUpdate) {
        pendingOTAUpdate = false; // Reset flag

        ESP_LOGI("OTA", "Starting manual OTA update check...");
        Serial.println("Starting manual OTA update...");
        esp_err_t ota_result = check_for_ota_update(true); // true = force update

        if (ota_result != ESP_OK)
        {
            Serial.printf(
                "OTA check failed: %s\n",
                esp_err_to_name(ota_result)
            );
        }
        else
        {
            rollbackMarkHealthy();
        }
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Yield to prevent WDT issues
}
