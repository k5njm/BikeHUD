#pragma once

#include <Arduino.h>

/**
 * X4 power button (GPIO3, active low) is software sleep/wake — not a hardware
 * cut-off. CrossPoint-style long-press (~0.5 s) enters deep sleep; the same
 * button wakes via ESP32-C3 GPIO wakeup.
 *
 * E-ink keeps the last image while the MCU sleeps (looks “frozen”).
 */

/** Hold time to enter sleep (ms). */
constexpr uint32_t kPowerSleepHoldMs = 500;

/**
 * Draw sleep splash, shut down BLE/panel, deep-sleep until power button.
 * Does not return.
 */
void power_enter_sleep() __attribute__((noreturn));
