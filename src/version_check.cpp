#include "version_check.h"

// --------------------------------------------------------------------------
// Internal helper: fetch JSON from the given URL and extract the "version" field.
// Returns empty string on any error (network, HTTP status, JSON parse, etc.).
// --------------------------------------------------------------------------
static String _fetchVersion(const String &fullUrl)
{
    HTTPClient http;
    String version;

    // ---- Open connection ---------------------------------------------------
    if (!http.begin(fullUrl)) {
        ESP_LOGW("VERSION_CHECK", "Failed to begin HTTP: %s", fullUrl.c_str());
        return "";
    }

    // ---- Perform GET -------------------------------------------------------
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        ESP_LOGW("VERSION_CHECK", "GET %s returned %d", fullUrl.c_str(), httpCode);
        http.end();
        return "";
    }

    // ---- Read payload -------------------------------------------------------
    String payload = http.getString();
    http.end();   // close connection early – we already have the data

    // ---- Parse JSON ---------------------------------------------------------
    StaticJsonDocument<256> doc;   // small enough for the version payload
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        ESP_LOGW("VERSION_CHECK", "JSON parse error (%s) payload: %s",
                 err.c_str(), payload.c_str());
        return "";
    }

    // ---- Extract "version" ---------------------------------------------------
    if (doc.containsKey("version")) {
        version = doc["version"].as<String>();
    } else {
        ESP_LOGW("VERSION_CHECK", "JSON missing \"version\" field: %s", payload.c_str());
    }

    return version;
}

// --------------------------------------------------------------------------
// Public: get version from the ESP32 that’s running this code.
// --------------------------------------------------------------------------
String getLocalVersion()
{
    // WiFi must already be connected.
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("VERSION_CHECK", "WiFi not connected – cannot query local version");
        return "";
    }

    IPAddress ip = WiFi.localIP();
    String url = "http://" + ip.toString() + "/version";

    return _fetchVersion(url);
}

// --------------------------------------------------------------------------
// Public: get version from a remote ESP32 (or any HTTP server exposing the
// same JSON). `baseUrl` should NOT contain a trailing slash.
// Example: "http://192.168.88.5"  or  "http://my-vpn-host.example.com"
// --------------------------------------------------------------------------
String getRemoteVersion(const char *baseUrl)
{
    if (baseUrl == nullptr) {
        ESP_LOGW("VERSION_CHECK", "NULL baseUrl supplied");
        return "";
    }

    // Ensure exactly one '/' between base URL and endpoint.
    String url = String(baseUrl);
    if (!url.endsWith("/")) {
        url += "/";
    }
    url += "version";

    return _fetchVersion(url);
}
