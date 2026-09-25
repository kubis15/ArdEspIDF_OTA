#ifndef VERSION_CHECK_H
#define VERSION_CHECK_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_log.h>

// Get the firmware version reported by this ESP32's own web server.
String getLocalVersion();

// Get the firmware version from a remote ESP32 reachable via baseUrl (e.g., VPN address).
String getRemoteVersion(const char *baseUrl);

#endif // VERSION_CHECK_H