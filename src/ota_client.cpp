#include <Arduino.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"

#include <ArduinoJson.h>

#include "ota_client.h"
#include "github_root_ca.h"


static const char *TAG = "OTA_CLIENT";


// ============================================================
// Manifest response buffer
// ============================================================

static char response_buffer[512];
static int response_len = 0;


// ============================================================
// HTTP EVENT HANDLER
// ============================================================

static esp_err_t _http_event_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
        case HTTP_EVENT_ON_DATA:
        {
            // esp_http_client_is_chunked_response() expects
            // the HTTP CLIENT handle, not the event handle.
            if (!esp_http_client_is_chunked_response(evt->client))
            {
                // Make sure there is enough room for received data
                // plus the terminating '\0'.
                if (response_len + evt->data_len <
                    (int)sizeof(response_buffer))
                {
                    memcpy(
                        response_buffer + response_len,
                        evt->data,
                        evt->data_len
                    );

                    response_len += evt->data_len;
                    response_buffer[response_len] = '\0';
                }
                else
                {
                    ESP_LOGE(
                        TAG,
                        "Manifest response exceeds buffer size"
                    );

                    return ESP_ERR_NO_MEM;
                }
            }

            break;
        }

        default:
            break;
    }

    return ESP_OK;
}


// ============================================================
// CHECK FOR OTA UPDATE
// ============================================================

esp_err_t check_for_ota_update()
{
    char manifest_url[256];

    // --------------------------------------------------------
    // Reset manifest response buffer
    // --------------------------------------------------------

    response_len = 0;
    memset(
        response_buffer,
        0,
        sizeof(response_buffer)
    );


    // --------------------------------------------------------
    // Validate OTA_URL
    // --------------------------------------------------------

    if (strlen(OTA_URL) == 0)
    {
        ESP_LOGE(TAG, "OTA_URL is empty");
        return ESP_ERR_INVALID_ARG;
    }


    // --------------------------------------------------------
    // Construct firmware.json URL
    //
    // Works with either:
    //
    // https://server/path
    //
    // or:
    //
    // https://server/path/
    // --------------------------------------------------------

    size_t url_len = strlen(OTA_URL);

    int written;

    if (OTA_URL[url_len - 1] == '/')
    {
        written = snprintf(
            manifest_url,
            sizeof(manifest_url),
            "%sfirmware.json",
            OTA_URL
        );
    }
    else
    {
        written = snprintf(
            manifest_url,
            sizeof(manifest_url),
            "%s/firmware.json",
            OTA_URL
        );
    }


    // Check for URL truncation.
    if (written < 0 ||
        written >= (int)sizeof(manifest_url))
    {
        ESP_LOGE(TAG, "Manifest URL is too long");
        return ESP_ERR_INVALID_SIZE;
    }


    ESP_LOGI(
        TAG,
        "Current firmware version: %s",
        CURRENT_VERSION
    );

    ESP_LOGI(
        TAG,
        "Manifest URL: %s",
        manifest_url
    );


    // --------------------------------------------------------
    // Configure HTTP client for firmware.json
    // --------------------------------------------------------

    esp_http_client_config_t config = {};

    config.url = manifest_url;
    config.cert_pem = GITHUB_ROOT_CA;
    config.timeout_ms = 8000;
    config.event_handler = _http_event_handler;


    // --------------------------------------------------------
    // Create HTTP client
    // --------------------------------------------------------

    esp_http_client_handle_t client =
        esp_http_client_init(&config);


    if (client == nullptr)
    {
        ESP_LOGE(
            TAG,
            "Failed to initialize HTTP client"
        );

        return ESP_FAIL;
    }


    // --------------------------------------------------------
    // Download firmware.json
    // --------------------------------------------------------

    ESP_LOGI(
        TAG,
        "Fetching OTA manifest..."
    );


    esp_err_t err =
        esp_http_client_perform(client);


    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "HTTP GET failed: %s",
            esp_err_to_name(err)
        );

        esp_http_client_cleanup(client);

        return err;
    }


    // --------------------------------------------------------
    // Check HTTP status code
    // --------------------------------------------------------

    int status_code =
        esp_http_client_get_status_code(client);


    ESP_LOGI(
        TAG,
        "Manifest HTTP status: %d",
        status_code
    );


    if (status_code != 200)
    {
        ESP_LOGE(
            TAG,
            "Manifest returned HTTP status %d",
            status_code
        );

        esp_http_client_cleanup(client);

        return ESP_FAIL;
    }


    // --------------------------------------------------------
    // Check response
    // --------------------------------------------------------

    if (response_len <= 0)
    {
        ESP_LOGE(
            TAG,
            "Manifest response is empty"
        );

        esp_http_client_cleanup(client);

        return ESP_FAIL;
    }


    ESP_LOGI(
        TAG,
        "Manifest received: %d bytes",
        response_len
    );


    // --------------------------------------------------------
    // Parse JSON manifest
    // --------------------------------------------------------

    JsonDocument doc;


    DeserializationError json_err =
        deserializeJson(
            doc,
            response_buffer
        );


    if (json_err)
    {
        ESP_LOGE(
            TAG,
            "Failed to parse JSON manifest: %s",
            json_err.c_str()
        );

        esp_http_client_cleanup(client);

        return ESP_FAIL;
    }


    // --------------------------------------------------------
    // Read required manifest fields
    // --------------------------------------------------------

    const char *remote_version =
        doc["version"];

    const char *download_url =
        doc["url"];


    // --------------------------------------------------------
    // Validate manifest
    // --------------------------------------------------------

    if (remote_version == nullptr)
    {
        ESP_LOGE(
            TAG,
            "Manifest is missing 'version'"
        );

        esp_http_client_cleanup(client);

        return ESP_ERR_INVALID_RESPONSE;
    }


    if (download_url == nullptr)
    {
        ESP_LOGE(
            TAG,
            "Manifest is missing 'url'"
        );

        esp_http_client_cleanup(client);

        return ESP_ERR_INVALID_RESPONSE;
    }


    if (strlen(remote_version) == 0)
    {
        ESP_LOGE(
            TAG,
            "Manifest 'version' is empty"
        );

        esp_http_client_cleanup(client);

        return ESP_ERR_INVALID_RESPONSE;
    }


    if (strlen(download_url) == 0)
    {
        ESP_LOGE(
            TAG,
            "Manifest 'url' is empty"
        );

        esp_http_client_cleanup(client);

        return ESP_ERR_INVALID_RESPONSE;
    }


    ESP_LOGI(
        TAG,
        "Remote firmware version: %s",
        remote_version
    );


    ESP_LOGI(
        TAG,
        "Firmware URL: %s",
        download_url
    );


    // --------------------------------------------------------
    // IMPORTANT:
    //
    // remote_version and download_url currently point into
    // ArduinoJson's JsonDocument.
    //
    // We need download_url later during OTA, so copy it into
    // our own buffer.
    // --------------------------------------------------------

    char firmware_url[384];


    int firmware_url_len = snprintf(
        firmware_url,
        sizeof(firmware_url),
        "%s",
        download_url
    );


    if (firmware_url_len < 0 ||
        firmware_url_len >= (int)sizeof(firmware_url))
    {
        ESP_LOGE(
            TAG,
            "Firmware URL is too long"
        );

        esp_http_client_cleanup(client);

        return ESP_ERR_INVALID_SIZE;
    }


    // --------------------------------------------------------
    // Compare versions BEFORE destroying JsonDocument
    // --------------------------------------------------------

    bool update_required =
        strcmp(
            remote_version,
            CURRENT_VERSION
        ) != 0;


    // --------------------------------------------------------
    // Manifest HTTP connection is no longer needed.
    // --------------------------------------------------------

    esp_http_client_cleanup(client);
    client = nullptr;


    // --------------------------------------------------------
    // Firmware is already current
    // --------------------------------------------------------

    if (!update_required)
    {
        ESP_LOGI(
            TAG,
            "Firmware is already up to date."
        );

        return ESP_OK;
    }


    // --------------------------------------------------------
    // New firmware detected
    // --------------------------------------------------------

    ESP_LOGI(
        TAG,
        "New firmware detected."
    );


    ESP_LOGI(
        TAG,
        "Current version: %s",
        CURRENT_VERSION
    );


    ESP_LOGI(
        TAG,
        "New version: %s",
        remote_version
    );


    ESP_LOGI(
        TAG,
        "Starting OTA update..."
    );


    // --------------------------------------------------------
    // Configure HTTPS OTA
    //
    // IMPORTANT:
    //
    // Your installed ESP-IDF version expects:
    //
    // esp_https_ota(
    //     const esp_http_client_config_t *
    // )
    //
    // Therefore this is intentionally
    // esp_http_client_config_t.
    // --------------------------------------------------------

    esp_http_client_config_t ota_config = {};

    ota_config.url = firmware_url;
    ota_config.cert_pem = GITHUB_ROOT_CA;
    ota_config.timeout_ms = 15000;


    // --------------------------------------------------------
    // Perform OTA
    // --------------------------------------------------------

    esp_err_t ota_ret =
        esp_https_ota(&ota_config);


    // --------------------------------------------------------
    // Check OTA result
    // --------------------------------------------------------

    if (ota_ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "OTA update failed: %s",
            esp_err_to_name(ota_ret)
        );

        return ota_ret;
    }


    // --------------------------------------------------------
    // OTA succeeded
    // --------------------------------------------------------

    ESP_LOGI(
        TAG,
        "OTA update successful!"
    );


    ESP_LOGI(
        TAG,
        "Rebooting..."
    );


    delay(1000);


    // Normally this function never returns after esp_restart().
    esp_restart();


    return ESP_OK;
}