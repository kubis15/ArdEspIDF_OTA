// src/version_check.cpp
#include <ESPAsyncWebServer.h>
#include "version_check.h"
#include <esp_log.h>

// 1. Define a global flag for manual OTA update requests
volatile bool pendingOTAUpdate = false;
// 1. Declare bootMillis globally at file scope
uint32_t bootMillis = 0;

// AsyncWebServer instance (global, lives for the whole program)
static AsyncWebServer server(80);   // listen on port 80

// startVersionServer() – registers the /version handler and starts the server
void startVersionServer()
{
    static bool started = false;
    if (started) return;
    started = true;
    server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Build the JSON completely in RAM each request
        JsonDocument doc;
        doc["version"] = String(CURRENT_VERSION);   // compile‑time version
        doc["ip"]      = WiFi.localIP().toString();
        doc["mac"]     = WiFi.macAddress();
        // uptime = seconds elapsed since boot
        doc["uptime"]  = (millis() - bootMillis) / 1000;
        char buffer[256];
    serializeJson(doc, buffer, sizeof(buffer));

    ESP_LOGI("VERSION", "Sending %s", buffer);
    request->send(200, "application/json", buffer);
    });
    
    // 2. HTTP Endpoint Handler
    server.on("/OTAnow", HTTP_GET, [](AsyncWebServerRequest *request) {
        ESP_LOGI("OTA", "Manual update triggered via HTTP GET /OTAnow");
        
        // Set flag for main loop execution
        pendingOTAUpdate = true;
        
        // Respond immediately to free up the HTTP connection
        request->send(200, "text/plain", "OTA Update Queued. Starting shortly...");
    });

    server.begin();                         // launch async server
    ESP_LOGI("VERSION", "Async /version and /OTAnow endpoints started on port 80");
}