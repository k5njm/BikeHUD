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
