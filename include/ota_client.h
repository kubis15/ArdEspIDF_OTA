#pragma once

#include "esp_err.h"

/**
 * Checks the OTA manifest and performs an OTA update using ESP-IDF APIs.
 *
 * Returns:
 *   ESP_OK  - manifest checked successfully and firmware is already
 *             up to date, or OTA update completed successfully.
 *
 *   Other esp_err_t value - an error occurred.
 */
esp_err_t check_for_ota_update();