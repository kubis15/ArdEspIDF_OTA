#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
///#include <FastLED.h>
#include "ota_client.h"

// --- Wi‑Fi credentials ---
const char* WIFI_SSID = "Moby_2.4_58D0C8";
const char* WIFI_PASS = "B8FBB358D0C8";

// --- NeoPixel configuration ---
#define LED_PIN 48 ///48          // YD‑ESP32‑S3 onboard WS2812 (IO48)
#define NUMPIXELS 1         // One RGB LED only
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
// FastLED array
///CRGB leds[NUMPIXELS];

// --- Helper: rainbow color wheel for Adafruit NeoPixel ---
uint32_t wheel(byte pos) {
    pos = 255 - pos;
    if (pos < 85) {
        return pixels.Color(255 - pos * 3, 0, pos * 3);
    } else if (pos < 170) {
        pos -= 85;
        return pixels.Color(0, pos * 3, 255 - pos * 3);
    } else {
        pos -= 170;
        return pixels.Color(pos * 3, 255 - pos * 3, 0);
    }
}

void setup() {
    //neopixelWrite(RGB_BUILTIN, 255, 0, 0);
    Serial.begin(115200);
    delay(1000);

    Serial.printf("\n--- Starting Firmware v%s ---\n", (CURRENT_VERSION));
    Serial.println("Boot reason: " + String(esp_reset_reason()));

    Serial.printf("Flash: %u MB\n", ESP.getFlashChipSize() / 1024 / 1024);
    Serial.printf("PSRAM: %u MB\n", ESP.getPsramSize() / 1024 / 1024);

    Serial.println("RGB ON");

    // pin, red, green, blue
    neopixelWrite(LED_PIN, 255, 255, 255);

    delay(1000);

    Serial.println("RGB OFF");

    // pin, red, green, blue
    neopixelWrite(LED_PIN, 1, 1, 1);


    // Initialize NeoPixel (Adafruit) and FastLED
    pixels.begin();
    pixels.setBrightness(20);
    pixels.clear();
    pixels.show();
    delay(10000);

    Serial.println("NeoPixel and FastLED initialized");

    // Connect to Wi‑Fi (commented out for now)
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        pixels.setPixelColor(0, pixels.Color(0, 0, 50)); // blue while connecting
        pixels.show();
    }
    Serial.println("\nWiFi Connected!");
    // Indicate connection (simulated)
    ///pixels.setPixelColor(0, pixels.Color(0, 50, 0)); // green when connected
    ///pixels.show();

    // Run OTA update check
    //check_and_perform_ota();
    esp_err_t ota_result = check_for_ota_update();

    if (ota_result != ESP_OK)
    {
        Serial.printf(
            "OTA check failed: %s\n",
            esp_err_to_name(ota_result)
        );
    }
}

void loop() {
    // Example rainbow animation using Adafruit NeoPixel helper (commented out)
    for (int i = 0; i < 256; i++) {
        pixels.setPixelColor(0, wheel(i));
        pixels.show();
        delay(20);
    }

    // Cycle colors using FastLED
    ///Serial.println("RED");
    ///leds[0] = CRGB::Red;
    ///FastLED.show();
    ///delay(2000);

    Serial.println("GREEN");
    ///leds[0] = CRGB::Green;
    ///FastLED.show();
    ///delay(2000);

    ///Serial.println("BLUE");
    ///leds[0] = CRGB::Blue;
    ///FastLED.show();
    delay(2000);

    // Turn off LED
    Serial.println("OFF");
    ///leds[0] = CRGB::Black;
    ///FastLED.show();
    ///    delay(2000);
}
