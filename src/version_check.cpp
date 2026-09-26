// src/version_check.cpp
#include <ESPAsyncWebServer.h>
#include "version_check.h"
#include <esp_log.h>

//#include <HTTPClient.h>
//#include <ArduinoJson.h>

// 1. Define a global flag for manual OTA update requests
volatile bool pendingOTAUpdate = false;
// 1. Declare bootMillis globally at file scope
uint32_t bootMillis = 0;

static String _fetchVersion(const String &fullUrl) {
    HTTPClient http;
    String version;

    if (!http.begin(fullUrl)) {
        ESP_LOGW("VERSION_CHECK", "Failed to begin HTTP: %s", fullUrl.c_str());
        return "";
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        ESP_LOGW("VERSION_CHECK", "GET %s returned %d", fullUrl.c_str(), httpCode);
        http.end();
        return "";
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc; // use the non‑deprecated type
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        ESP_LOGW("VERSION_CHECK", "JSON parse error (%s) payload: %s",
                 err.c_str(), payload.c_str());
        return "";
    }

    if (doc["version"].is<const char*>()) {
        version = String(doc["version"].as<const char*>());
    } else {
        ESP_LOGW("VERSION_CHECK", "JSON missing \"version\" field: %s", payload.c_str());
    }
    return version;
}

String getLocalVersion() {
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("VERSION_CHECK", "WiFi not connected – cannot query local version");
        return "";
    }
    IPAddress ip = WiFi.localIP();
    return _fetchVersion("http://" + ip.toString() + "/version");
}

String getRemoteVersion(const char *baseUrl) {
    if (!baseUrl) {
        ESP_LOGW("VERSION_CHECK", "NULL baseUrl supplied");
        return "";
    }
    String url = String(baseUrl);
    if (!url.endsWith("/")) url += "/";
    url += "version";
    return _fetchVersion(url);
}
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

//+++++++++++++++++++++++++++//+++++++++++++++++++++++++++
// Check for OTA update by comparing local and remote versions
// Do i need this really? I already have check_for_ota_update()
// in ota_client.cpp, which does the same thing and more.
//+++++++++++++++++++++++++++//+++++++++++++++++++++++++++

// bool checkForOTAUpdate() {
//     if (WiFi.status() != WL_CONNECTED) {
//         ESP_LOGW("OTA_CHECK", "WiFi not connected");
//         return false;
//     }

//     // Pass the build flag directly
//     String remoteVersion = _fetchVersion(OTA_URL);
//     if (remoteVersion.length() == 0) {
//         ESP_LOGE("OTA_CHECK", "Failed to obtain remote version from %s", OTA_URL);
//         return false;
//     }

//     String localVersion = String(CURRENT_VERSION);

//     ESP_LOGI("OTA_CHECK", "Local: %s | Remote: %s", localVersion.c_str(), remoteVersion.c_str());

//     if (remoteVersion != localVersion) {
//         ESP_LOGI("OTA_CHECK", "New version detected! Preparing update...");
//         return true;
//     }

//     ESP_LOGI("OTA_CHECK", "Firmware is up to date.");
//     return false;
// }
//+++++++++++++++++++++++++++//+++++++++++++++++++++++++++