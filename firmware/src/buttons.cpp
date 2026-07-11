#include "buttons.h"

#include "board_pins.h"

/**
 * ADC ladder decode — lifted from CrossPoint open-x4-sdk InputManager.
 *
 * Recorded ADC values from real devices
 * BACK CONF LEFT RGHT   UP DOWN
 * 3597 2760 1530    6 2300    6
 * 3470 2666 1480    6 2222    5
 * 3470 2655 1470    3 2205    3
 *
 * Averages → range midpoints → bins (open ranges, device-tolerant):
 *   ADC1: Back / Confirm / Left / Right
 *   ADC2: Up / Down
 *   Idle (no press) ≈ 4095; treat anything above ADC_NO_BUTTON as none.
 */

namespace {

// Same bins as CrossPoint InputManager::ADC_RANGES_*.
// If ADC is between ranges[i+1] and ranges[i] (exclusive lower, inclusive upper),
// button index i is pressed.
constexpr int kAdcNoButton = 3900;
constexpr int kRanges1[] = {kAdcNoButton, 3100, 2090, 750, INT32_MIN}; // 4 front
constexpr int kRanges2[] = {kAdcNoButton, 1120, INT32_MIN};            // 2 side
constexpr int kNumFront = 4;
constexpr int kNumSide = 2;

BoardButton last_stable = BoardButton::None;
BoardButton last_emitted = BoardButton::None;
uint32_t last_change_ms = 0;
constexpr uint32_t kDebounceMs = 20; // CrossPoint uses 5; keep a bit more margin

int buttonFromAdc(int adcValue, const int *ranges, int numButtons) {
  for (int i = 0; i < numButtons; i++) {
    if (ranges[i + 1] < adcValue && adcValue <= ranges[i]) {
      return i;
    }
  }
  return -1;
}

const char *btnName(BoardButton b) {
  switch (b) {
  case BoardButton::Right:
    return "Right";
  case BoardButton::Left:
    return "Left";
  case BoardButton::Confirm:
    return "Confirm";
  case BoardButton::Back:
    return "Back";
  case BoardButton::VolumeUp:
    return "VolUp";
  case BoardButton::VolumeDown:
    return "VolDown";
  case BoardButton::Power:
    return "Power";
  default:
    return "None";
  }
}

BoardButton decode() {
  if (digitalRead(PIN_BTN_POWER) == LOW) {
    return BoardButton::Power;
  }

  // Front ladder GPIO1: Back, Confirm, Left, Right (CrossPoint order).
  const int a1 = analogRead(PIN_BTN_ADC1);
  const int front = buttonFromAdc(a1, kRanges1, kNumFront);
  if (front == 0) {
    return BoardButton::Back;
  }
  if (front == 1) {
    return BoardButton::Confirm;
  }
  if (front == 2) {
    return BoardButton::Left;
  }
  if (front == 3) {
    return BoardButton::Right;
  }

  // Side ladder GPIO2: Up, Down (CrossPoint names Vol-ish physical sides).
  const int a2 = analogRead(PIN_BTN_ADC2);
  const int side = buttonFromAdc(a2, kRanges2, kNumSide);
  if (side == 0) {
    return BoardButton::VolumeUp;
  }
  if (side == 1) {
    return BoardButton::VolumeDown;
  }

  return BoardButton::None;
}

uint32_t g_power_down_since = 0;

} // namespace

void buttons_begin() {
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);
  pinMode(PIN_BTN_ADC1, INPUT);
  pinMode(PIN_BTN_ADC2, INPUT);
  // Critical: CrossPoint uses 11dB so ladder voltages land in measured bins.
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);
}

BoardButton buttons_poll() {
  const BoardButton now = decode();
  const uint32_t t = millis();

  if (now != last_stable) {
    last_stable = now;
    last_change_ms = t;
    return BoardButton::None;
  }

  if ((t - last_change_ms) < kDebounceMs) {
    return BoardButton::None;
  }

  // Power is handled via hold timing in main (sleep), not as a short edge.
  if (now == BoardButton::Power) {
    return BoardButton::None;
  }

  if (now != BoardButton::None && now != last_emitted) {
    last_emitted = now;
    const int a1 = analogRead(PIN_BTN_ADC1);
    const int a2 = analogRead(PIN_BTN_ADC2);
    Serial.printf("[btn] %s  a1=%d a2=%d\n", btnName(now), a1, a2);
    return now;
  }

  if (now == BoardButton::None) {
    last_emitted = BoardButton::None;
  }
  return BoardButton::None;
}

uint32_t buttons_power_held_ms() {
  if (digitalRead(PIN_BTN_POWER) == LOW) {
    if (g_power_down_since == 0) {
      g_power_down_since = millis();
      if (g_power_down_since == 0) {
        g_power_down_since = 1;
      }
    }
    return millis() - g_power_down_since;
  }
  g_power_down_since = 0;
  return 0;
}

void buttons_power_reset_hold() { g_power_down_since = 0; }
