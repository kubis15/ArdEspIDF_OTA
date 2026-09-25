#ifndef VERSION_CHECK_H
#define VERSION_CHECK_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_log.h>

// Return the version string from the local ESP32’s /version endpoint.
String getLocalVersion();

// Return the version string from a remote ESP32 reachable via baseUrl.
String getRemoteVersion(const char *baseUrl);

// Start the AsyncWebServer that serves /version (call from setup()).
void startVersionServer();

#endif // VERSION_CHECK_H