#pragma once

#include <Arduino.h>

// ============================================================
// Software rollback for stock Arduino-ESP32.
//
// The precompiled Arduino bootloader is built WITHOUT
// CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE, so the ESP-IDF
// PENDING_VERIFY / esp_ota_mark_app_valid_* mechanism is a
// no-op on this toolchain. This module approximates it in the
// application layer instead:
//
//  - After a successful OTA flash, the image is armed as
//    "pending validation" in NVS (rollbackArmPendingValidation).
//  - Every boot while pending increments an attempt counter
//    (rollbackCheckOnBoot). Crash/panic loops exceed the budget
//    and the app flips the boot partition back itself.
//  - The self-test is the existing boot flow: WiFi connected AND
//    the OTA manifest fetched over TLS. Passing calls
//    rollbackMarkHealthy(); failing WiFi on a trial image rolls
//    back immediately (rollbackOnTrialConnectFailed).
//  - A rolled-back version is remembered in NVS and never
//    auto-reinstalled (rollbackIsRejectedVersion) - otherwise the
//    boot OTA check would re-download the broken build forever.
//    A forced update (/OTAnow) clears the block deliberately.
//
// Known limit: a crash in global constructors / framework init,
// before setup() runs, cannot be caught in software. That gap
// needs the real bootloader mechanism (hybrid arduino+espidf
// build with CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE).
// ============================================================

// Call FIRST in setup() (right after Serial is up).
// Handles the GPIO force-rollback override and the
// boot-attempt budget. May not return (restarts into the
// other OTA slot).
void rollbackCheckOnBoot();

// Call when WiFi failed on a boot. Rolls back (never returns)
// if this image is on trial; returns immediately otherwise.
void rollbackOnTrialConnectFailed();

// Call right before esp_restart() after a successful OTA flash.
void rollbackArmPendingValidation();

// Call when the self-test passed (WiFi up + manifest fetched).
void rollbackMarkHealthy();

// True if this manifest version was previously rolled back.
bool rollbackIsRejectedVersion(const char *version);

// Forget the rejected version (used by forced updates).
void rollbackClearRejected();
