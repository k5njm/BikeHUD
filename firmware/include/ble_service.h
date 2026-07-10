#pragma once

#include <Arduino.h>

/** Start NimBLE peripheral advertising BikeHUD service. */
void ble_service_begin();

/** Call from loop — currently a no-op place for house-keeping. */
void ble_service_loop();

/** True if a central is currently connected. */
bool ble_service_is_connected();
