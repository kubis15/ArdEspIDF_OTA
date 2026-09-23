#pragma once

#include "github_root_ca.h"
#include "esp_err.h"


#ifndef OTA_URL
#define OTA_URL "https://kubis15.github.io/ArdEspIDF_OTA/"
#endif

/**
 * Checks the OTA manifest and performs an OTA update using ESP-IDF APIs.
 * Returns ESP_OK on success (or when already up‑to‑date), otherwise an error code.
 */
esp_err_t check_and_perform_ota(void);
