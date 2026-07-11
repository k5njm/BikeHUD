#pragma once

#include <Arduino.h>

/**
 * X4 power management (inspired by CrossPoint HalPowerManager):
 *
 * - GPIO3 power button, active low — long-press to sleep / wake
 * - GPIO13 battery latch — held LOW in deep sleep (MCU unpowered on battery)
 * - Auto-sleep after inactivity (default 10 minutes, same ballpark as CrossPoint)
 * - Soft-sleep while USB is attached (deep sleep + USB-JTAG often instant-wakes)
 */

/** Hold time for power button sleep / wake (ms). */
constexpr uint32_t kPowerSleepHoldMs = 500;

/** No button / BLE / packet activity for this long → auto-sleep. 0 = disabled. */
constexpr uint32_t kAutoSleepIdleMs = 10UL * 60UL * 1000UL; // 10 min

/** Record user or network activity (resets auto-sleep timer). */
void power_note_activity();

/** Call from loop: auto-sleep if idle long enough. */
void power_poll_auto_sleep(uint32_t now_ms);

/**
 * Sleep now: splash, stop BLE, deep sleep (battery) or soft-sleep (USB).
 * Returns only after soft-wake.
 */
void power_enter_sleep();
