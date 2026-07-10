#pragma once

#include <Arduino.h>
#include "telemetry.h"

/** Init SPI + panel; draw chrome once. */
void hud_begin();

/**
 * Render based on telemetry freshness and current page.
 * Safe to call often; only paints e-ink when dirty or periodic full refresh due.
 * @param ble_linked true when a BLE central is connected (shown on status page).
 */
void hud_update(const Telemetry &tel, uint32_t now_ms, bool ble_linked);

/** Cycle page 0..N-1. */
void hud_next_page();
void hud_prev_page();

uint8_t hud_current_page();
