#include "buttons.h"

#include "board_pins.h"

namespace {

// Thresholds from xteink-x4-sample (approximate resistor-ladder midpoints).
constexpr int kTol = 120;

constexpr int kRight = 3;
constexpr int kLeft = 1470;
constexpr int kConfirm = 2655;
constexpr int kBack = 3470;
constexpr int kVolDown = 3;
constexpr int kVolUp = 2205;

BoardButton last_stable = BoardButton::None;
BoardButton last_emitted = BoardButton::None;
uint32_t last_change_ms = 0;
constexpr uint32_t kDebounceMs = 40;

bool near(int v, int target) {
  return abs(v - target) <= kTol;
}

BoardButton decode() {
  const int a1 = analogRead(PIN_BTN_ADC1);
  const int a2 = analogRead(PIN_BTN_ADC2);

  if (digitalRead(PIN_BTN_POWER) == LOW) {
    return BoardButton::Power;
  }
  if (near(a1, kRight)) {
    return BoardButton::Right;
  }
  if (near(a1, kLeft)) {
    return BoardButton::Left;
  }
  if (near(a1, kConfirm)) {
    return BoardButton::Confirm;
  }
  if (near(a1, kBack)) {
    return BoardButton::Back;
  }
  if (near(a2, kVolUp)) {
    return BoardButton::VolumeUp;
  }
  if (near(a2, kVolDown) && a2 < 200) {
    // VolDown and floating low can alias — only treat as press if clearly low
    // while a1 is idle (not also pressed). Best-effort.
    if (a1 > 3800) {
      return BoardButton::VolumeDown;
    }
  }
  return BoardButton::None;
}

} // namespace

void buttons_begin() {
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);
  // ADC pins default to analog on ESP32-C3
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
    return now;
  }

  if (now == BoardButton::None) {
    last_emitted = BoardButton::None;
  }
  return BoardButton::None;
}

uint32_t buttons_power_held_ms() {
  static uint32_t down_since = 0;
  if (digitalRead(PIN_BTN_POWER) == LOW) {
    if (down_since == 0) {
      down_since = millis();
      if (down_since == 0) {
        down_since = 1;
      }
    }
    return millis() - down_since;
  }
  down_since = 0;
  return 0;
}
