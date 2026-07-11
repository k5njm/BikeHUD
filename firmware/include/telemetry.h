#pragma once

#include <Arduino.h>
#include "bike_hud_protocol.h"

/** Rolling samples for sparkline + avg/hi/lo (1 Hz pushes). */
struct MetricSeries {
  static constexpr uint8_t kCap = 48;
  uint8_t samples[kCap]{};
  uint8_t count = 0;
  uint8_t head = 0; // next write index

  void clear() {
    count = 0;
    head = 0;
  }

  void push(uint8_t v) {
    samples[head] = v;
    head = (uint8_t)((head + 1) % kCap);
    if (count < kCap) {
      count++;
    }
  }

  /** Oldest → newest into out[]; returns n. */
  uint8_t copyChronological(uint8_t *out, uint8_t max_n) const {
    if (count == 0 || max_n == 0) {
      return 0;
    }
    const uint8_t n = count < max_n ? count : max_n;
    const uint8_t start =
        (uint8_t)((head + kCap - count) % kCap);
    for (uint8_t i = 0; i < n; i++) {
      out[i] = samples[(start + i) % kCap];
    }
    return n;
  }

  void stats(uint8_t *avg, uint8_t *lo, uint8_t *hi) const {
    if (count == 0) {
      *avg = *lo = *hi = 0;
      return;
    }
    uint16_t sum = 0;
    uint8_t mn = 255, mx = 0;
    uint8_t tmp[kCap];
    const uint8_t n = copyChronological(tmp, kCap);
    for (uint8_t i = 0; i < n; i++) {
      sum = (uint16_t)(sum + tmp[i]);
      if (tmp[i] < mn)
        mn = tmp[i];
      if (tmp[i] > mx)
        mx = tmp[i];
    }
    *avg = (uint8_t)(sum / n);
    *lo = mn;
    *hi = mx;
  }
};

/**
 * Live ride state on the X4: last good packet + receive timestamp +
 * wall clock (v2) and metric history for HR/cadence charts.
 */
struct Telemetry {
  BikeHudPacketV1 packet{};
  // Wall clock from hub (local time); valid if clock_valid.
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  uint8_t day_of_week = 0;
  bool clock_valid = false;

  MetricSeries hr_hist{};
  MetricSeries cad_hist{};

  uint32_t recv_ms = 0;
  bool has_packet = false;
  uint32_t write_count = 0;

  void apply(const BikeHudPacketV1 &p, uint32_t now_ms) {
    apply_v2_body(p, /*clk*/ nullptr, now_ms);
  }

  void apply_v2(const BikeHudPacketV2 &p, uint32_t now_ms) {
    apply_v2_body(p.body, &p, now_ms);
  }

  void apply_v2_body(const BikeHudPacketV1 &p, const BikeHudPacketV2 *full,
                     uint32_t now_ms) {
    packet = p;
    recv_ms = now_ms;
    has_packet = true;
    write_count++;

    // Accept wall clock from v2 trailer when flag set, or when fields look sane
    // (older/buggy hubs may omit FLAG_CLOCK_VALID).
    if (full && p.version == BIKE_HUD_PROTOCOL_VERSION_V2 &&
        full->year >= 2020 && full->year < 2100 && full->month >= 1 &&
        full->month <= 12 && full->day >= 1 && full->day <= 31 &&
        full->hour <= 23 && full->minute <= 59 && full->day_of_week <= 6) {
      year = full->year;
      month = full->month;
      day = full->day;
      hour = full->hour;
      minute = full->minute;
      second = full->second;
      day_of_week = full->day_of_week;
      clock_valid = true;
    }

    if (p.flags & BIKE_HUD_FLAG_HR_VALID) {
      hr_hist.push(p.hr_bpm);
    }
    if ((p.flags & BIKE_HUD_FLAG_CADENCE_VALID) &&
        p.cadence_rpm != BIKE_HUD_UNKNOWN_U8) {
      cad_hist.push(p.cadence_rpm);
    }
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

extern Telemetry g_telemetry;
