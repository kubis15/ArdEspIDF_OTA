// src/rollback.cpp
#include "rollback.h"

#include <Preferences.h>
#include "esp_ota_ops.h"

// Hardware override: pull this pin LOW (jumper/button to GND) while the app
// starts to force a revert to the other OTA slot - works even when the
// current image is already marked healthy.
//
// GPIO4 is free on the YD-ESP32-S3: strapping pins are 0/3/45/46 and octal
// PSRAM owns 33-37. Deliberately NOT GPIO0 (BOOT button): that is a
// strapping pin, and LOW during reset enters the serial bootloader, so the
// app would never run to see it.
#ifndef ROLLBACK_FORCE_PIN
#define ROLLBACK_FORCE_PIN 4
#endif

// Boots a trial image may consume before the app rolls itself back.
static const uint8_t MAX_BOOT_ATTEMPTS = 3;

static Preferences rb;

// ------------------------------------------------------------
// Flip the boot partition to the other OTA slot and restart.
// Stays on the current image (and returns) if the other slot
// does not hold a valid app image.
// ------------------------------------------------------------
static void rollbackNow(const char *reason)
{
    const esp_partition_t *other =
        esp_ota_get_next_update_partition(NULL);

    esp_app_desc_t desc;

    if (other == nullptr ||
        esp_ota_get_partition_description(other, &desc) != ESP_OK)
    {
        Serial.printf(
            "Rollback requested (%s) but the other slot holds no valid app - staying on this image.\n",
            reason
        );
        return;
    }

    // Remember this version so the boot OTA check never
    // auto-reinstalls it (rollback -> re-download loop).
    rb.begin("rollback", false);
    rb.putString("rejected", CURRENT_VERSION);
    rb.putBool("pending", false);
    rb.putUChar("attempts", 0);
    rb.end();

    Serial.printf(
        "ROLLBACK (%s): rebooting into previous image in slot %s\n",
        reason,
        other->label
    );
    Serial.flush();

    esp_ota_set_boot_partition(other);
    esp_restart();
}

void rollbackCheckOnBoot()
{
    // --- hardware override ---
    pinMode(ROLLBACK_FORCE_PIN, INPUT_PULLUP);
    delay(50);                                    // let the pull-up settle

    if (digitalRead(ROLLBACK_FORCE_PIN) == LOW)
    {
        rollbackNow("GPIO override");
        // falls through only if the other slot is empty
    }

    // --- boot-attempt budget for a trial image ---
    rb.begin("rollback", false);
    bool    pending  = rb.getBool("pending", false);
    uint8_t attempts = rb.getUChar("attempts", 0);

    if (pending)
    {
        attempts++;
        rb.putUChar("attempts", attempts);
        Serial.printf(
            "Firmware on trial: boot attempt %u of %u\n",
            attempts,
            MAX_BOOT_ATTEMPTS
        );
    }
    rb.end();

    if (pending && attempts > MAX_BOOT_ATTEMPTS)
    {
        rollbackNow("boot-attempt budget exceeded");
    }
}

void rollbackOnTrialConnectFailed()
{
    rb.begin("rollback", true);
    bool pending = rb.getBool("pending", false);
    rb.end();

    if (pending)
    {
        // The previous image could connect; this one can't. Don't sit in
        // the recovery AP on a suspect build - return to the known-good
        // image instead.
        rollbackNow("WiFi connect failed on trial image");
    }
}

void rollbackArmPendingValidation()
{
    rb.begin("rollback", false);
    rb.putBool("pending", true);
    rb.putUChar("attempts", 0);
    rb.end();

    Serial.println("Next boot runs on trial - self-test must pass to keep it.");
}

void rollbackMarkHealthy()
{
    rb.begin("rollback", false);
    if (rb.getBool("pending", false))
    {
        rb.putBool("pending", false);
        rb.putUChar("attempts", 0);
        Serial.println("Self-test passed - firmware marked as known-good.");
    }
    rb.end();
}

bool rollbackIsRejectedVersion(const char *version)
{
    if (version == nullptr)
    {
        return false;
    }

    rb.begin("rollback", true);
    // isKey() guard: Preferences logs a spurious [E] line when
    // getString() is asked for a key that does not exist yet.
    String rejected = rb.isKey("rejected") ? rb.getString("rejected", "")
                                           : String();
    rb.end();

    return rejected.length() > 0 && rejected.equals(version);
}

void rollbackClearRejected()
{
    rb.begin("rollback", false);
    if (rb.isKey("rejected"))
    {
        rb.remove("rejected");
        Serial.println("Cleared rejected-version block (forced update).");
    }
    rb.end();
}
