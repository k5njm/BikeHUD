#pragma once

#include <Arduino.h>
#include "bike_hud_protocol.h"

/**
 * Live ride state on the X4: last good packet + receive timestamp.
 * Age-based staleness mirrors stravaV10 locator behaviour.
 */
struct Telemetry {
  BikeHudPacketV1 packet{};
  uint32_t recv_ms = 0;
  bool has_packet = false;
  uint32_t write_count = 0;

  void apply(const BikeHudPacketV1 &p, uint32_t now_ms) {
    packet = p;
    recv_ms = now_ms;
    has_packet = true;
    write_count++;
  }

  uint32_t age_ms(uint32_t now_ms) const {
    if (!has_packet) {
      return UINT32_MAX;
    }
    return now_ms - recv_ms;
  }

  enum class Freshness : uint8_t { Live, Weak, HardStale, Empty };

  Freshness freshness(uint32_t now_ms) const {
    if (!has_packet) {
      return Freshness::Empty;
    }
    const uint32_t a = age_ms(now_ms);
    if (a > BIKE_HUD_STALE_HARD_MS) {
      return Freshness::HardStale;
    }
    if (a > BIKE_HUD_STALE_WEAK_MS) {
      return Freshness::Weak;
    }
    return Freshness::Live;
  }
};

// Global state — single writer from BLE callback / demo tick.
extern Telemetry g_telemetry;
