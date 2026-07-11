#pragma once

#include <Arduino.h>
#include "bike_hud_protocol.h"

/** Rolling samples for sparkline + avg/hi/lo (1 Hz pushes while live). */
struct MetricSeries {
  static constexpr uint8_t kCap = 48;
  uint8_t samples[kCap]{};
  uint8_t count = 0;
  uint8_t head = 0;

  void clear() {
    count = 0;
    head = 0;
  }

  void push(uint8_t v) {
    samples[head] = v;
    head = (uint8_t)((head + 1) % kCap);
    if (count < kCap)
      count++;
  }

  uint8_t copyChronological(uint8_t *out, uint8_t max_n) const {
    if (count == 0 || max_n == 0)
      return 0;
    const uint8_t n = count < max_n ? count : max_n;
    const uint8_t start = (uint8_t)((head + kCap - count) % kCap);
    for (uint8_t i = 0; i < n; i++)
      out[i] = samples[(start + i) % kCap];
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
 * Software wall clock: set by a 16-byte TIME_SYNC message, then free-runs
 * from millis(). Independent of telemetry freshness / BLE link.
 */
struct WallClock {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  uint8_t day_of_week = 0;
  uint32_t set_ms = 0;
  uint32_t set_unix_s = 0; // seconds-of-day-ish cumulative for advance
  bool valid = false;

  static uint8_t daysInMonth(uint16_t y, uint8_t m) {
    static const uint8_t kDom[] = {0, 31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12)
      return 30;
    if (m == 2) {
      const bool leap =
          (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
      return leap ? 29 : 28;
    }
    return kDom[m];
  }

  void set(uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi,
           uint8_t s, uint8_t dow, uint32_t now_ms) {
    year = y;
    month = mo;
    day = d;
    hour = h;
    minute = mi;
    second = s;
    day_of_week = dow;
    set_ms = now_ms;
    valid = (y >= 2020 && y < 2100 && mo >= 1 && mo <= 12 && d >= 1 &&
             d <= 31 && h <= 23 && mi <= 59 && s <= 59 && dow <= 6);
  }

  /** Advance from set_ms to now_ms and fill current fields. */
  void snapshot(uint32_t now_ms, uint16_t *y, uint8_t *mo, uint8_t *d,
                uint8_t *h, uint8_t *mi, uint8_t *s, uint8_t *dow) const {
    if (!valid) {
      *y = *mo = *d = *h = *mi = *s = *dow = 0;
      return;
    }
    uint32_t elapsed_s = (now_ms - set_ms) / 1000u;
    uint16_t yy = year;
    uint8_t m = month, dd = day, hh = hour, mm = minute, ss = second;
    uint8_t dw = day_of_week;

    ss = (uint8_t)(ss + (elapsed_s % 60u));
    elapsed_s /= 60u;
    if (ss >= 60) {
      ss = (uint8_t)(ss - 60);
      elapsed_s++;
    }
    mm = (uint8_t)(mm + (elapsed_s % 60u));
    elapsed_s /= 60u;
    if (mm >= 60) {
      mm = (uint8_t)(mm - 60);
      elapsed_s++;
    }
    hh = (uint8_t)(hh + (elapsed_s % 24u));
    uint32_t days = elapsed_s / 24u;
    if (hh >= 24) {
      hh = (uint8_t)(hh - 24);
      days++;
    }
    while (days > 0) {
      const uint8_t dim = daysInMonth(yy, m);
      if (dd < dim) {
        dd++;
        days--;
        dw = (uint8_t)((dw + 1) % 7);
      } else {
        dd = 1;
        m++;
        if (m > 12) {
          m = 1;
          yy++;
        }
        days--;
        dw = (uint8_t)((dw + 1) % 7);
      }
    }
    *y = yy;
    *mo = m;
    *d = dd;
    *h = hh;
    *mi = mm;
    *s = ss;
    *dow = dw;
  }
};

struct Telemetry {
  BikeHudPacketV1 packet{};
  WallClock clock{};
  MetricSeries hr_hist{};
  MetricSeries cad_hist{};

  uint32_t recv_ms = 0;
  bool has_packet = false;
  uint32_t write_count = 0;

  void applyTelemetry(const BikeHudPacketV1 &p, uint32_t now_ms) {
    packet = p;
    recv_ms = now_ms;
    has_packet = true;
    write_count++;

    if (p.flags & BIKE_HUD_FLAG_HR_VALID)
      hr_hist.push(p.hr_bpm);
    if ((p.flags & BIKE_HUD_FLAG_CADENCE_VALID) &&
        p.cadence_rpm != BIKE_HUD_UNKNOWN_U8)
      cad_hist.push(p.cadence_rpm);
  }

  void applyTimeSync(const BikeHudTimeSync &t, uint32_t now_ms) {
    clock.set(t.year, t.month, t.day, t.hour, t.minute, t.second,
              t.day_of_week, now_ms);
  }

  /** Convenience for older call sites. */
  void apply(const BikeHudPacketV1 &p, uint32_t now_ms) {
    applyTelemetry(p, now_ms);
  }

  uint32_t age_ms(uint32_t now_ms) const {
    if (!has_packet)
      return UINT32_MAX;
    return now_ms - recv_ms;
  }

  enum class Freshness : uint8_t { Live, Weak, HardStale, Empty };

  Freshness freshness(uint32_t now_ms) const {
    if (!has_packet)
      return Freshness::Empty;
    const uint32_t a = age_ms(now_ms);
    if (a > BIKE_HUD_STALE_HARD_MS)
      return Freshness::HardStale;
    if (a > BIKE_HUD_STALE_WEAK_MS)
      return Freshness::Weak;
    return Freshness::Live;
  }
};

extern Telemetry g_telemetry;
