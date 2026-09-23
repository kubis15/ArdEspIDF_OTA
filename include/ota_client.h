#pragma once

#include "github_root_ca.h"
#include "esp_err.h"

#ifndef CURRENT_VERSION
#define CURRENT_VERSION "1.0.0"
#else
#undef CURRENT_VERSION
#define CURRENT_VERSION "1.0.0"
#endif

#ifndef OTA_URL
#define OTA_URL "https://<username>.github.io/<repo>"
#endif

/**
 * Checks the OTA manifest and performs an OTA update using ESP-IDF APIs.
 * Returns ESP_OK on success (or when already up‑to‑date), otherwise an error code.
 */
esp_err_t check_and_perform_ota(void);
