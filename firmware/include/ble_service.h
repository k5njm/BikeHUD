#pragma once

#include <Arduino.h>

/** Start NimBLE peripheral advertising BikeHUD service. */
void ble_service_begin();

/** Call from loop — currently a no-op place for house-keeping. */
void ble_service_loop();

/** True if a central is currently connected. */
bool ble_service_is_connected();

/**
 * Stop advertising and drop connections for sleep.
 * Does **not** call NimBLEDevice::deinit (that crashes when a central is
 * connected — disconnect restarts advertising mid-free). Resume with
 * ble_service_resume_from_sleep() or ble_service_begin().
 */
void ble_service_shutdown_for_sleep();

/** Restart advertising after soft-sleep (if stack still alive). */
void ble_service_resume_from_sleep();
