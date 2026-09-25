// src/main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include "ota_client.h"
#include "github_root_ca.h"
#include "version_check.h"          // <<< NEW – brings in the /version server

// ---------------------------------------------------------------
// Your existing Wi‑Fi credentials (replace with your own)
// ---------------------------------------------------------------
const char* WIFI_SSID = "Moby_2.4_58D0C8";
const char* WIFI_PASS = "YOUR_PASSWORD"; // replace with real password

// ---------------------------------------------------------------
// Setup ---------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // -----------------------------------------------------------
    // Wait for Wi‑Fi
    // -----------------------------------------------------------
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());

    // -----------------------------------------------------------
    // START THE /version ENDPOINT (RAM‑only, dynamic JSON)
    // -----------------------------------------------------------
    startVersionServer();   // <<< NEW – registers the async handler

    // -----------------------------------------------------------
    // OPTIONAL: Show the JSON that will be returned on the next request
    // -----------------------------------------------------------
    {
        // Capture boot time for uptime calculation
        static uint32_t bootMillis = millis();
        JsonDocument doc(256);
        doc["version"] = String(PIO_PROJECT_VERSION);
        doc["ip"]      = WiFi.localIP().toString();
        doc["mac"]     = WiFi.macAddress();
        doc["uptime"]  = (millis() - bootMillis) / 1000; // seconds since boot

        String payload;
        serializeJson(doc, payload);
        Serial.println("Sample /version payload (generated on request):");
        Serial.println(payload);
    }

    // -----------------------------------------------------------
    // Your original initialization code (NeoPixel, OTA, etc.)
    // -----------------------------------------------------------
    Serial.printf("\n--- Starting Firmware v%s ---\n", (CURRENT_VERSION));
    // ... rest of your existing setup (LED init, time sync, OTA check, etc.) ...
}

// ---------------------------------------------------------------
// Main loop ------------------------------------------------------
void loop()
{
    // Your existing loop logic (rainbow animation, OTA status checks, etc.)
    // ----------------------------------------------------------------
    // Example: you could periodically call the remote version helper:
    //   String remote = getRemoteVersion("http://192.168.88.50");
    //   // compare with CURRENT_VERSION, trigger OTA if needed, etc.
    // ----------------------------------------------------------------
}
