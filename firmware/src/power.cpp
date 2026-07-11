#include "power.h"

#include "board_pins.h"
#include "ble_service.h"
#include "buttons.h"
#include "hud.h"

#include <NimBLEDevice.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

/**
 * CrossPoint-aligned power path for Xteink X4 / X3 family:
 *
 *   - Wait for power button release
 *   - Drive GPIO13 battery latch LOW and hold it through deep sleep
 *     (on battery the MCU is fully powered off; button hard-powers the rail)
 *   - GPIO wake on power button is mainly for USB-powered wake
 *   - Soft-sleep when USB is present (JTAG keeps waking deep sleep)
 */

namespace {

uint32_t g_last_activity_ms = 0;

bool usb_vbus_likely_present() {
  pinMode(PIN_USB_DETECT, INPUT);
  return digitalRead(PIN_USB_DETECT) == HIGH;
}

void wait_power_released() {
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

void latch_power_rail_off_for_deep_sleep() {
  // CrossPoint: GPIO13 → battery latch MOSFET, low = cut MCU power on battery.
  gpio_num_t latch = (gpio_num_t)PIN_PWR_LATCH;
  gpio_reset_pin(latch);
  gpio_set_direction(latch, GPIO_MODE_OUTPUT);
  gpio_set_level(latch, 0);
  gpio_hold_en(latch);
  gpio_deep_sleep_hold_en();
}

bool try_deep_sleep() {
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);
  wait_power_released();
  delay(200);

  if (digitalRead(PIN_BTN_POWER) == LOW) {
    Serial.println("[power] button still low — skip deep sleep");
    return false;
  }

  // Isolate GPIOs; hold latch low across sleep (CrossPoint startDeepSleep).
  esp_sleep_config_gpio_isolate();
  latch_power_rail_off_for_deep_sleep();

  gpio_num_t pwr = (gpio_num_t)PIN_BTN_POWER;
  gpio_reset_pin(pwr);
  gpio_set_direction(pwr, GPIO_MODE_INPUT);
  gpio_set_pull_mode(pwr, GPIO_PULLUP_ONLY);
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);

  const uint64_t mask = 1ULL << PIN_BTN_POWER;
  esp_err_t err =
      esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_LOW);
  if (err != ESP_OK) {
    Serial.printf("[power] gpio wakeup enable failed: %d\n", (int)err);
    gpio_hold_dis((gpio_num_t)PIN_PWR_LATCH);
    return false;
  }

  Serial.println("[power] deep sleep (latch low, wake on power btn)");
  Serial.flush();
  delay(30);
  esp_deep_sleep_start();
  return true;
}

void soft_sleep_loop() {
  Serial.println("[power] soft-sleep — hold power ~0.5s to wake");
  Serial.flush();

  while (true) {
    delay(40);
    if (buttons_power_held_ms() >= kPowerSleepHoldMs) {
      Serial.println("[power] soft-wake");
      wait_power_released();
      return;
    }
  }
}

} // namespace

void power_note_activity() { g_last_activity_ms = millis(); }

void power_poll_auto_sleep(uint32_t now_ms) {
  if (kAutoSleepIdleMs == 0) {
    return;
  }
  if (g_last_activity_ms == 0) {
    g_last_activity_ms = now_ms;
    return;
  }
  if (now_ms - g_last_activity_ms >= kAutoSleepIdleMs) {
    Serial.printf("[power] auto-sleep after %lu ms idle\n",
                  (unsigned long)kAutoSleepIdleMs);
    power_enter_sleep();
  }
}

void power_enter_sleep() {
  Serial.println("[power] sleep requested");
  Serial.flush();

#if !defined(BIKE_HUD_DEMO)
  NimBLEDevice::deinit(true);
#endif

  // One full refresh splash via existing panel (no second driver).
  hud_show_sleep_splash();

  wait_power_released();
  delay(150);

  // Prefer CrossPoint-style deep sleep when unplugged.
  if (!usb_vbus_likely_present()) {
    if (try_deep_sleep()) {
      // not reached
    }
  } else {
    Serial.println("[power] USB present — soft-sleep (deep sleep unstable on CDC)");
  }

  soft_sleep_loop();

  // Soft-wake only: restore button + BLE + UI.
  gpio_hold_dis((gpio_num_t)PIN_PWR_LATCH);
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);
  pinMode(PIN_PWR_LATCH, OUTPUT);
  digitalWrite(PIN_PWR_LATCH, HIGH); // ensure rail stays on after soft-wake

#if !defined(BIKE_HUD_DEMO)
  ble_service_begin();
#endif
  hud_force_full_redraw();
  power_note_activity();
  Serial.println("[power] resumed");
}
