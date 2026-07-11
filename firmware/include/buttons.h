#pragma once

#include <Arduino.h>

enum class BoardButton : uint8_t {
  None = 0,
  Right,
  Left,
  Confirm,
  Back,
  VolumeUp,
  VolumeDown,
  Power,
};

void buttons_begin();

/** Debounced edge: returns a button only on press transition. */
BoardButton buttons_poll();

/**
 * How long the power button (GPIO3) has been held continuously, or 0 if up.
 * Used for CrossPoint-style long-press sleep (see power.h).
 */
uint32_t buttons_power_held_ms();

/**
 * Clear the power hold timer (call on sleep / wake edges).
 * Without this, a long soft-sleep leaves `down_since` stale; the first
 * LOW sample after wake reports an enormous hold and immediately re-sleeps.
 */
void buttons_power_reset_hold();
