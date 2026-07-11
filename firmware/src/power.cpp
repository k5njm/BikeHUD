#include "power.h"

#include "board_pins.h"
#include "ble_service.h"
#include "buttons.h"
#include "hud.h"

#include <NimBLEDevice.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

/**
 * Power button = GPIO3, active LOW (same as CrossPoint / FreeInk).
 *
 * Deep sleep on ESP32-C3 with USB-Serial/JTAG attached often wakes
 * immediately (USB activity), which looks like: flash → back to UI.
 * So we:
 *  1) Soft-sleep by default (splash + idle loop, wake on long-press)
 *  2) Optionally try deep sleep when USB is not present
 *
 * Soft-sleep still drops BLE and stops the ride UI; e-ink holds the splash.
 */

namespace {

bool usb_vbus_likely_present() {
  // Optional USB detect pin on X4 (polarity varies; treat high as "maybe USB").
  pinMode(PIN_USB_DETECT, INPUT);
  // Also: if host keeps CDC open, prefer soft-sleep.
  return digitalRead(PIN_USB_DETECT) == HIGH;
}

void wait_power_released() {
  // Debounce: must see HIGH for a stretch before we consider released.
  uint32_t high_since = 0;
  while (true) {
    if (digitalRead(PIN_BTN_POWER) == HIGH) {
      if (high_since == 0) {
        high_since = millis();
      } else if (millis() - high_since > 200) {
        return;
      }
    } else {
      high_since = 0;
    }
    delay(10);
  }
}

bool try_deep_sleep() {
  // Configure pull-up so the line idles HIGH while asleep; wake on press (LOW).
  gpio_num_t pin = (gpio_num_t)PIN_BTN_POWER;
  gpio_reset_pin(pin);
  gpio_set_direction(pin, GPIO_MODE_INPUT);
  gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);

  wait_power_released();
  delay(250);

  // If still low, abort deep sleep (would instant-wake).
  if (digitalRead(PIN_BTN_POWER) == LOW) {
    Serial.println("[power] button still low — soft-sleep only");
    return false;
  }

  const uint64_t mask = 1ULL << PIN_BTN_POWER;
  esp_err_t err =
      esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_LOW);
  if (err != ESP_OK) {
    Serial.printf("[power] gpio wakeup enable failed: %d\n", (int)err);
    return false;
  }

  Serial.println("[power] deep sleep now");
  Serial.flush();
  delay(50);
  esp_deep_sleep_start();
  return true; // never reached
}

void soft_sleep_loop() {
  Serial.println("[power] soft-sleep (hold power ~0.5s to wake)");
  Serial.flush();

  // Idle until long-press power again.
  while (true) {
    delay(30);
    if (buttons_power_held_ms() < kPowerSleepHoldMs) {
      continue;
    }
    Serial.println("[power] wake long-press detected");
    wait_power_released();
    return;
  }
}

} // namespace

void power_enter_sleep() {
  Serial.println("[power] sleep requested");
  Serial.flush();

#if !defined(BIKE_HUD_DEMO)
  // Tear down radio before sleeping.
  NimBLEDevice::deinit(true);
#endif

  // Splash with the *existing* panel driver (no second GxEPD2 / re-init).
  hud_show_sleep_splash();

  wait_power_released();
  delay(150);

  // Deep sleep only when USB looks absent — otherwise it instant-wakes.
  const bool try_deep = !usb_vbus_likely_present();
  if (try_deep) {
    if (try_deep_sleep()) {
      // not reached
    }
  } else {
    Serial.println("[power] USB present — using soft-sleep");
  }

  // Soft-sleep (or deep-sleep fallback): low-activity loop until long-press.
  soft_sleep_loop();

  // Wake path: restore pull-up, BLE, force UI redraw.
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);
#if !defined(BIKE_HUD_DEMO)
  ble_service_begin();
#endif
  hud_force_full_redraw();
  Serial.println("[power] woke (soft-sleep)");
}
