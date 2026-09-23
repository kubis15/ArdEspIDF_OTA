#include "ota_client.h"
#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "cJSON.h"
#include "github_root_ca.h"

static const char *TAG = "OTA_CLIENT";
#define BUF_SIZE 512

esp_err_t check_and_perform_ota(void) {
    char manifest_url[256];
    snprintf(manifest_url, sizeof(manifest_url), "%s/firmware.json", OTA_URL);

    ESP_LOGI(TAG, "Checking for update manifest: %s", manifest_url);

    // 1. Configure and fetch manifest using ESP-IDF HTTP client
    esp_http_client_config_t config = {};
    config.url = manifest_url;
    config.cert_pem = GITHUB_ROOT_CA;
    config.timeout_ms = 8000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "Manifest request returned status %d", status_code);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    char buffer[BUF_SIZE] = {0};
    int read_len = esp_http_client_read(client, buffer, BUF_SIZE - 1);
    esp_http_client_cleanup(client);

    if (read_len <= 0) {
        ESP_LOGE(TAG, "Failed to read manifest data");
        return ESP_FAIL;
    }

    // 2. Parse version manifest using cJSON
    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    cJSON *version_item = cJSON_GetObjectItem(root, "version");
    cJSON *url_item = cJSON_GetObjectItem(root, "url");

    if (!cJSON_IsString(version_item) || !cJSON_IsString(url_item)) {
        ESP_LOGE(TAG, "Invalid manifest structure");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Current Version: %s | Remote Version: %s", CURRENT_VERSION, version_item->valuestring);

    if (strcmp(version_item->valuestring, CURRENT_VERSION) == 0) {
        ESP_LOGI(TAG, "Firmware is already up to date.");
        cJSON_Delete(root);
        return ESP_OK;
    }

    // 3. Execute update using ESP-IDF HTTPS OTA component
    ESP_LOGI(TAG, "New firmware available. Downloading from: %s", url_item->valuestring);

    esp_http_client_config_t ota_http_config = {};
    ota_http_config.url = url_item->valuestring;
    ota_http_config.cert_pem = GITHUB_ROOT_CA;
    ota_http_config.timeout_ms = 15000;

    // Pass the HTTP client config struct directly for ESP-IDF v4.4 / Arduino 2.x
    esp_err_t ota_ret = esp_https_ota(&ota_http_config);
    cJSON_Delete(root);

    if (ota_ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Flash successful! Restarting system...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA execution failed: %s", esp_err_to_name(ota_ret));
        return ota_ret;
    }

    return ESP_OK;
}