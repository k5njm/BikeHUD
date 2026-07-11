#include "power.h"

#include "board_pins.h"
#include "ble_service.h"
#include "buttons.h"
#include "hud.h"

/**
 * Reliable sleep first; CrossPoint deep-sleep+latch is opt-in later.
 *
 * Deep sleep + GPIO13 latch was bouncing: flash then cold-boot UI (USB-JTAG
 * and/or latch). Soft-sleep keeps the MCU running in a tight wait, splash on
 * e-ink, BLE off, until a *new* long-press of power.
 */

namespace {

uint32_t g_last_activity_ms = 0;
/** Ignore sleep re-entry for a short window after soft-wake (button bounce). */
uint32_t g_sleep_guard_until_ms = 0;
constexpr uint32_t kPostWakeSleepGuardMs = 800;

bool power_is_down() { return digitalRead(PIN_BTN_POWER) == LOW; }

/** Block until button has been HIGH for `stable_ms`. */
void wait_power_up_stable(uint32_t stable_ms = 250) {
  uint32_t high_since = 0;
  while (true) {
    if (!power_is_down()) {
      if (high_since == 0) {
        high_since = millis();
      } else if (millis() - high_since >= stable_ms) {
        return;
      }
    } else {
      high_since = 0;
    }
    delay(10);
  }
}

/**
 * Wait for a fresh long-press: button must be up, then held >= hold_ms.
 * Ignores the press that initiated sleep.
 */
void wait_fresh_long_press(uint32_t hold_ms) {
  wait_power_up_stable(200);
  Serial.println("[power] armed — hold power to wake");
  Serial.flush();

  uint32_t down_since = 0;
  while (true) {
    if (power_is_down()) {
      if (down_since == 0) {
        down_since = millis();
      } else if (millis() - down_since >= hold_ms) {
        Serial.println("[power] wake press accepted");
        wait_power_up_stable(150);
        return;
      }
    } else {
      down_since = 0;
    }
    delay(20);
  }
}

} // namespace

void power_note_activity() { g_last_activity_ms = millis(); }

bool power_sleep_guard_active(uint32_t now_ms) {
  if (g_sleep_guard_until_ms == 0) {
    return false;
  }
  // Handle millis wrap: if until is in the past window, clear it.
  if ((int32_t)(g_sleep_guard_until_ms - now_ms) <= 0) {
    g_sleep_guard_until_ms = 0;
    return false;
  }
  return true;
}

void power_poll_auto_sleep(uint32_t now_ms) {
  // Use the freshest clock: buttons call power_note_activity() mid-loop with
  // millis(), which can be *after* the stale `now` captured at loop entry.
  // Treating that as unsigned (now - activity) underflows to ~4e9 ms and
  // immediately trips the 10‑minute idle sleep.
  const uint32_t now = millis();
  (void)now_ms;

  if (power_sleep_guard_active(now)) {
    return;
  }
  if (kAutoSleepIdleMs == 0) {
    return;
  }
  if (g_last_activity_ms == 0) {
    g_last_activity_ms = now;
    return;
  }
  // Signed delta: if activity is ahead of `now` (same-tick race), idle is 0.
  const int32_t idle_ms = (int32_t)(now - g_last_activity_ms);
  if (idle_ms >= (int32_t)kAutoSleepIdleMs) {
    Serial.printf("[power] auto-sleep after %lu ms idle\n",
                  (unsigned long)kAutoSleepIdleMs);
    power_enter_sleep();
  }
}

void power_enter_sleep() {
  Serial.println("[power] === SLEEP BEGIN ===");
  Serial.flush();

  // Drop any pre-sleep hold state so a later sample later cannot look "held
  // for hours". Soft-sleep blocks in this function for a long time.
  buttons_power_reset_hold();

  // Keep battery latch ON for soft-sleep (do not cut the rail).
  pinMode(PIN_PWR_LATCH, OUTPUT);
  digitalWrite(PIN_PWR_LATCH, HIGH);
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);

#if !defined(BIKE_HUD_DEMO)
  // Safe pause — never NimBLEDevice::deinit while a central is connected
  // (that double-frees heap; see serial: heap_caps_free assert).
  ble_service_shutdown_for_sleep();
#endif

  Serial.println("[power] drawing splash");
  Serial.flush();
  hud_show_sleep_splash();
  Serial.println("[power] splash done");
  Serial.flush();

  // User may still be holding the button from the sleep gesture.
  Serial.println("[power] wait for release");
  Serial.flush();
  wait_power_up_stable(300);
  buttons_power_reset_hold();
  delay(100);

  // Stay here until a *new* long-press. Splash remains on e-ink.
  wait_fresh_long_press(kPowerSleepHoldMs);

  // Wake
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);
  pinMode(PIN_PWR_LATCH, OUTPUT);
  digitalWrite(PIN_PWR_LATCH, HIGH);

  // Critical: clear hold timer before returning to loop. Otherwise the first
  // bounce-LOW reuses a pre-sleep `down_since` and held_ms looks enormous →
  // immediate re-entry into this function (splash bounce).
  buttons_power_reset_hold();

  Serial.println("[power] restoring display + BLE");
  Serial.flush();
  hud_wake_from_sleep();
#if !defined(BIKE_HUD_DEMO)
  ble_service_resume_from_sleep();
#endif
  power_note_activity();
  g_sleep_guard_until_ms = millis() + kPostWakeSleepGuardMs;
  if (g_sleep_guard_until_ms == 0) {
    g_sleep_guard_until_ms = 1; // keep "active" if millis wraps to 0
  }
  Serial.println("[power] === AWAKE ===");
  Serial.flush();
}
