// src/version_check.cpp
#include "version_check.h"
#include <esp_log.h>

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
