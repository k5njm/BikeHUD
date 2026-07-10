#include "hud.h"

#include "board_pins.h"

#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <GxEPD2_BW.h>
#include <SPI.h>
#include <stdio.h>
#include <string.h>

// GDEQ0426T82 800x480 SSD1677 — X4 panel
static GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT>
    display(GxEPD2_426_GDEQ0426T82(/*CS=*/PIN_EPD_CS, /*DC=*/PIN_EPD_DC,
                                  /*RST=*/PIN_EPD_RST, /*BUSY=*/PIN_EPD_BUSY));

namespace {

constexpr uint8_t kPageCount = 3;
uint8_t g_page = 0;
uint32_t g_last_full_ms = 0;
uint32_t g_last_drawn_ms = 0;
uint32_t g_last_write_seen = 0;
uint16_t g_partials_since_full = 0;
Telemetry::Freshness g_last_fresh = Telemetry::Freshness::Empty;
bool g_last_ble_linked = false;
bool g_page_dirty = false;

// Partial updates leave faint "ghost" residual. Full waveform (invert flash)
// clears it — but is only needed after many partials, not on a static desk
// screen. During a live ~1 Hz ride: full clean every ~3 min if we've actually
// been partial-refreshing.
constexpr uint32_t kFullRefreshIntervalMs = 180000; // 3 min min between fulls
constexpr uint16_t kPartialsBeforeFull = 90;        // ~90 s of 1 Hz partials
constexpr uint32_t kMinPartialIntervalMs = 900;

// Portrait after setRotation(3): 480 × 800
// Layout inspired by Cycplus-style bike computers.

// Display units only — wire protocol stays metric (cm/s, m). Prefer imperial
// for US; later: NVS / iOS settings char can override this default.
constexpr bool kUseImperial = true;

const char *freshnessLabel(Telemetry::Freshness f) {
  switch (f) {
  case Telemetry::Freshness::Live:
    return "LIVE";
  case Telemetry::Freshness::Weak:
    return "WEAK";
  case Telemetry::Freshness::HardStale:
    return "STALE";
  default:
    return "WAITING";
  }
}

/** Instantaneous speed, one decimal — mph or km/h from speed_cm_s. */
void formatSpeed(char *buf, size_t n, const BikeHudPacketV1 &p) {
  if (kUseImperial) {
    // mph*10 ≈ cm/s * 0.223694 → * 2237 / 10000
    const uint32_t x10 =
        ((uint32_t)p.speed_cm_s * 2237u) / 10000u;
    snprintf(buf, n, "%u.%u", (unsigned)(x10 / 10u), (unsigned)(x10 % 10u));
  } else {
    const uint16_t x10 = bike_hud_speed_kmh_x10(p.speed_cm_s);
    snprintf(buf, n, "%u.%u", x10 / 10, x10 % 10);
  }
}

const char *speedUnitLabel() { return kUseImperial ? "MPH" : "KMH"; }

/** Ride distance — miles (2 dp) or km (2 dp). */
void formatDistance(char *buf, size_t n, uint16_t distance_m) {
  if (kUseImperial) {
    // miles * 100 = m * 100 / 1609.344 ≈ m * 1000 / 16093
    const uint32_t x100 = ((uint32_t)distance_m * 1000u) / 16093u;
    snprintf(buf, n, "%u.%02u", (unsigned)(x100 / 100u),
             (unsigned)(x100 % 100u));
  } else {
    // km * 100 from metres
    const uint32_t x100 = (uint32_t)distance_m / 10u;
    snprintf(buf, n, "%u.%02u", (unsigned)(x100 / 100u),
             (unsigned)(x100 % 100u));
  }
}

const char *distanceLabel() { return kUseImperial ? "DST mi" : "DST km"; }

/** Elevation — feet or metres. */
void formatElev(char *buf, size_t n, int16_t elev_m) {
  if (kUseImperial) {
    // ft ≈ m * 3.28084 → * 328 / 100
    const int32_t ft = ((int32_t)elev_m * 328) / 100;
    snprintf(buf, n, "%d", (int)ft);
  } else {
    snprintf(buf, n, "%d", (int)elev_m);
  }
}

const char *elevLabel() { return kUseImperial ? "ELEV ft" : "ELEV m"; }

const char *avgLabel() { return kUseImperial ? "AVG mph" : "AVG km/h"; }

/** Elapsed workout time (not wall clock). Always 24h-style HH:MM:SS when ≥1h. */
void formatElapsed(char *buf, size_t n, uint16_t seconds) {
  const uint16_t m = seconds / 60;
  const uint16_t s = seconds % 60;
  const uint16_t h = m / 60;
  const uint16_t mm = m % 60;
  if (h > 0) {
    snprintf(buf, n, "%u:%02u:%02u", h, mm, s);
  } else {
    snprintf(buf, n, "%u:%02u", mm, s);
  }
}

// --- 7-segment style chunky digits (hero speed) -------------------------
// Segment map: 0=A top, 1=B upper-right, 2=C lower-right, 3=D bottom,
//              4=E lower-left, 5=F upper-left, 6=G middle
static const uint8_t kSeg[12] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
    0x00, // blank
    0x40, // dash (middle only)
};

void fillHBar(int16_t x, int16_t y, int16_t w, int16_t t) {
  display.fillRect(x, y, w, t, GxEPD_BLACK);
}

void fillVBar(int16_t x, int16_t y, int16_t t, int16_t h) {
  display.fillRect(x, y, t, h, GxEPD_BLACK);
}

void drawSegDigit(int16_t x, int16_t y, int16_t dw, int16_t dh, uint8_t code) {
  const int16_t t = (dw < 40) ? 6 : 10; // stroke thickness
  const int16_t inset = t / 2;
  const int16_t mid_y = y + dh / 2 - t / 2;
  const int16_t inner_h = (dh - 3 * t) / 2;

  if (code & 0x01) // A top
    fillHBar(x + inset, y, dw - 2 * inset, t);
  if (code & 0x02) // B UR
    fillVBar(x + dw - t, y + inset, t, inner_h + t);
  if (code & 0x04) // C LR
    fillVBar(x + dw - t, mid_y, t, inner_h + t);
  if (code & 0x08) // D bottom
    fillHBar(x + inset, y + dh - t, dw - 2 * inset, t);
  if (code & 0x10) // E LL
    fillVBar(x, mid_y, t, inner_h + t);
  if (code & 0x20) // F UL
    fillVBar(x, y + inset, t, inner_h + t);
  if (code & 0x40) // G mid
    fillHBar(x + inset, mid_y, dw - 2 * inset, t);
}

uint8_t charToSeg(char c) {
  if (c >= '0' && c <= '9')
    return kSeg[c - '0'];
  if (c == '-')
    return kSeg[11];
  return kSeg[10]; // blank
}

/** Draw chunky speed string like "31.6" or "--.-" centered in box. */
void drawHeroNumber(int16_t box_x, int16_t box_y, int16_t box_w, int16_t box_h,
                    const char *text) {
  // Count glyphs (skip '.') for layout width.
  int glyphs = 0;
  for (const char *p = text; *p; ++p) {
    if (*p != '.')
      glyphs++;
  }
  if (glyphs < 1)
    glyphs = 1;

  const int16_t gap = 10;
  const int16_t dot_w = 14;
  // Estimate total width: glyphs*dw + gaps + dots
  int16_t dots = 0;
  for (const char *p = text; *p; ++p) {
    if (*p == '.')
      dots++;
  }
  // Prefer digit height ~ 55% of box
  int16_t dh = (int16_t)(box_h * 0.55f);
  if (dh < 80)
    dh = 80;
  if (dh > 200)
    dh = 200;
  int16_t dw = (int16_t)(dh * 0.58f);

  int16_t total =
      glyphs * dw + (glyphs - 1) * gap + dots * (dot_w + gap / 2);
  // Shrink if too wide
  if (total > box_w - 16) {
    const float scale = (float)(box_w - 16) / (float)total;
    dw = (int16_t)(dw * scale);
    dh = (int16_t)(dh * scale);
    total =
        glyphs * dw + (glyphs - 1) * gap + dots * (dot_w + gap / 2);
  }

  int16_t cx = box_x + (box_w - total) / 2;
  int16_t cy = box_y + (box_h - dh) / 2 + 8;

  for (const char *p = text; *p; ++p) {
    if (*p == '.') {
      display.fillCircle(cx + dot_w / 2, cy + dh - 12, 6, GxEPD_BLACK);
      cx += dot_w + gap / 2;
    } else {
      drawSegDigit(cx, cy, dw, dh, charToSeg(*p));
      cx += dw + gap;
    }
  }
}

void textSize(const GFXfont *font, const char *s, int16_t *w, int16_t *h) {
  display.setFont(font);
  int16_t x1, y1;
  uint16_t tw, th;
  display.getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
  *w = (int16_t)tw;
  *h = (int16_t)th;
}

void drawCentered(const GFXfont *font, int16_t cx, int16_t baseline_y,
                  const char *s) {
  display.setFont(font);
  int16_t x1, y1;
  uint16_t tw, th;
  display.getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor(cx - (int16_t)tw / 2 - x1, baseline_y);
  display.print(s);
}

void drawLabelLeft(const GFXfont *font, int16_t x, int16_t baseline_y,
                   const char *s) {
  display.setFont(font);
  display.setCursor(x, baseline_y);
  display.print(s);
}

/** Metric cell: thick border, small label top-left, big value centered. */
void drawMetricCell(int16_t x, int16_t y, int16_t w, int16_t h,
                    const char *label, const char *value,
                    const GFXfont *value_font) {
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  display.drawRect(x + 1, y + 1, w - 2, h - 2, GxEPD_BLACK);

  drawLabelLeft(&FreeSansBold9pt7b, x + 10, y + 20, label);

  int16_t vw, vh;
  textSize(value_font, value, &vw, &vh);
  const int16_t cx = x + w / 2;
  const int16_t cy = y + h / 2 + vh / 2 + 6;
  drawCentered(value_font, cx, cy, value);
}

void drawStatusBar(Telemetry::Freshness f, bool linked) {
  const int16_t W = display.width();
  // Top bar height 44
  display.drawRect(0, 0, W, 44, GxEPD_BLACK);
  display.drawRect(1, 1, W - 2, 42, GxEPD_BLACK);

  drawLabelLeft(&FreeSansBold12pt7b, 12, 30, "BikeHUD");

  // Freshness = packet age; LINK/ADV = BLE central connected vs advertising.
  char right[28];
  snprintf(right, sizeof(right), "%s · %s", freshnessLabel(f),
           linked ? "LINK" : "ADV");
  int16_t rw, rh;
  textSize(&FreeSansBold12pt7b, right, &rw, &rh);
  // Prefer full "WAITING · ADV"; shrink font only if it won't fit.
  const GFXfont *rf = &FreeSansBold12pt7b;
  if (rw > W / 2) {
    textSize(&FreeSansBold9pt7b, right, &rw, &rh);
    rf = &FreeSansBold9pt7b;
  }
  drawLabelLeft(rf, W - rw - 12, 30, right);
}

void paintPage0(const Telemetry &tel, bool show_values) {
  const int16_t W = display.width();
  const int16_t H = display.height();

  // Hero panel under status bar
  const int16_t hero_y = 44;
  const int16_t hero_h = 300;
  display.drawRect(0, hero_y, W, hero_h, GxEPD_BLACK);
  display.drawRect(1, hero_y + 1, W - 2, hero_h - 2, GxEPD_BLACK);

  drawLabelLeft(&FreeSansBold12pt7b, 14, hero_y + 28, "NOW");
  {
    const char *unit = speedUnitLabel();
    int16_t uw, uh;
    textSize(&FreeSansBold12pt7b, unit, &uw, &uh);
    drawLabelLeft(&FreeSansBold12pt7b, W - uw - 14, hero_y + 28, unit);
  }

  char speed[16];
  if (show_values) {
    formatSpeed(speed, sizeof(speed), tel.packet);
  } else {
    snprintf(speed, sizeof(speed), "--.-");
  }
  drawHeroNumber(8, hero_y + 36, W - 16, hero_h - 48, speed);

  // 2×2 metric grid
  const int16_t grid_y = hero_y + hero_h;
  const int16_t grid_h = H - grid_y;
  const int16_t cell_w = W / 2;
  const int16_t cell_h = grid_h / 2;

  char hr[12], time_s[16], cad[12], dist[16];

  if (show_values && (tel.packet.flags & BIKE_HUD_FLAG_HR_VALID)) {
    snprintf(hr, sizeof(hr), "%u", tel.packet.hr_bpm);
  } else {
    snprintf(hr, sizeof(hr), "--");
  }

  if (show_values) {
    formatElapsed(time_s, sizeof(time_s), tel.packet.elapsed_s);
  } else {
    snprintf(time_s, sizeof(time_s), "--:--");
  }

  if (show_values && (tel.packet.flags & BIKE_HUD_FLAG_CADENCE_VALID) &&
      tel.packet.cadence_rpm != BIKE_HUD_UNKNOWN_U8) {
    snprintf(cad, sizeof(cad), "%u", tel.packet.cadence_rpm);
  } else {
    snprintf(cad, sizeof(cad), "--");
  }

  if (show_values) {
    formatDistance(dist, sizeof(dist), tel.packet.distance_m);
  } else {
    snprintf(dist, sizeof(dist), "--.--");
  }

  drawMetricCell(0, grid_y, cell_w, cell_h, "HR", hr, &FreeSansBold24pt7b);
  drawMetricCell(cell_w - 1, grid_y, cell_w + 1, cell_h, "TIME", time_s,
                 &FreeSansBold24pt7b);
  drawMetricCell(0, grid_y + cell_h - 1, cell_w, cell_h + 1, "CAD", cad,
                 &FreeSansBold24pt7b);
  drawMetricCell(cell_w - 1, grid_y + cell_h - 1, cell_w + 1, cell_h + 1,
                 distanceLabel(), dist, &FreeSansBold24pt7b);
}

void paintPage1(const Telemetry &tel, bool show_values) {
  const int16_t W = display.width();
  const int16_t H = display.height();
  const int16_t top = 44;
  const int16_t body_h = H - top;
  const int16_t cell_w = W / 2;
  const int16_t cell_h = body_h / 2;

  char elev[16], avg[16], batt[12], hub[16];

  if (show_values) {
    formatElev(elev, sizeof(elev), tel.packet.elev_m);
  } else {
    snprintf(elev, sizeof(elev), "----");
  }

  if (show_values && tel.packet.elapsed_s > 0) {
    // Average speed from distance/time → fake a cm/s then reuse formatter.
    const uint32_t avg_cm_s =
        ((uint32_t)tel.packet.distance_m * 100u) / tel.packet.elapsed_s;
    BikeHudPacketV1 tmp = tel.packet;
    tmp.speed_cm_s =
        avg_cm_s > 65535u ? 65535u : (uint16_t)avg_cm_s;
    formatSpeed(avg, sizeof(avg), tmp);
  } else {
    snprintf(avg, sizeof(avg), "--.-");
  }

  if (show_values && tel.packet.batt_pct != BIKE_HUD_UNKNOWN_U8) {
    snprintf(batt, sizeof(batt), "%u%%", tel.packet.batt_pct);
  } else {
    snprintf(batt, sizeof(batt), "--");
  }

  if (tel.has_packet) {
    if (tel.packet.hub_type == BIKE_HUD_HUB_IPHONE) {
      snprintf(hub, sizeof(hub), "iPhone");
    } else if (tel.packet.hub_type == BIKE_HUD_HUB_WATCH) {
      snprintf(hub, sizeof(hub), "Watch");
    } else {
      snprintf(hub, sizeof(hub), "?");
    }
  } else {
    snprintf(hub, sizeof(hub), "--");
  }

  drawMetricCell(0, top, cell_w, cell_h, elevLabel(), elev, &FreeSansBold24pt7b);
  drawMetricCell(cell_w - 1, top, cell_w + 1, cell_h, avgLabel(), avg,
                 &FreeSansBold24pt7b);
  drawMetricCell(0, top + cell_h - 1, cell_w, cell_h + 1, "HUB BATT", batt,
                 &FreeSansBold24pt7b);
  drawMetricCell(cell_w - 1, top + cell_h - 1, cell_w + 1, cell_h + 1, "HUB",
                 hub, &FreeSansBold18pt7b);
}

void paintPage2(const Telemetry &tel, Telemetry::Freshness f, bool linked) {
  const int16_t W = display.width();
  const int16_t H = display.height();
  const int16_t top = 44;

  display.drawRect(0, top, W, H - top, GxEPD_BLACK);
  display.drawRect(1, top + 1, W - 2, H - top - 2, GxEPD_BLACK);

  char line[48];
  int16_t y = top + 40;

  drawLabelLeft(&FreeSansBold18pt7b, 20, y, freshnessLabel(f));
  y += 48;

  snprintf(line, sizeof(line), "BLE  %s",
           linked ? "LINK (phone connected)" : "ADV (waiting for phone)");
  drawLabelLeft(&FreeSansBold12pt7b, 20, y, line);
  y += 36;

  const char *hub = "hub  --";
  if (tel.has_packet) {
    if (tel.packet.hub_type == BIKE_HUD_HUB_IPHONE)
      hub = "hub  iPhone";
    else if (tel.packet.hub_type == BIKE_HUD_HUB_WATCH)
      hub = "hub  Watch";
  }
  drawLabelLeft(&FreeSansBold12pt7b, 20, y, hub);
  y += 36;

  if (tel.has_packet && tel.packet.gps_acc_m != BIKE_HUD_UNKNOWN_U8) {
    snprintf(line, sizeof(line), "GPS  +/- %um", tel.packet.gps_acc_m);
  } else {
    snprintf(line, sizeof(line), "GPS  --");
  }
  drawLabelLeft(&FreeSansBold12pt7b, 20, y, line);
  y += 36;

  snprintf(line, sizeof(line), "pkts %lu", (unsigned long)tel.write_count);
  drawLabelLeft(&FreeSansBold12pt7b, 20, y, line);
  y += 36;

  snprintf(line, sizeof(line), "heap %u", (unsigned)ESP.getFreeHeap());
  drawLabelLeft(&FreeSansBold12pt7b, 20, y, line);
  y += 36;

  snprintf(line, sizeof(line), "page %u/%u", g_page + 1, kPageCount);
  drawLabelLeft(&FreeSansBold12pt7b, 20, y, line);

  if (tel.packet.flags & BIKE_HUD_FLAG_PAUSED) {
    drawCentered(&FreeSansBold24pt7b, W / 2, H - 40, "PAUSED");
  }
}

void paintFrame(const Telemetry &tel, Telemetry::Freshness f, bool linked,
                bool partial) {
  // 3 = 180° from previous rotation(1); portrait 480×800 on X4.
  display.setRotation(3);
  display.setTextColor(GxEPD_BLACK);

  if (partial) {
    display.setPartialWindow(0, 0, display.width(), display.height());
  } else {
    display.setFullWindow();
  }

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawStatusBar(f, linked);

    const bool show =
        (f == Telemetry::Freshness::Live || f == Telemetry::Freshness::Weak);

    switch (g_page) {
    case 0:
      paintPage0(tel, show);
      break;
    case 1:
      paintPage1(tel, show);
      break;
    default:
      paintPage2(tel, f, linked);
      break;
    }
  } while (display.nextPage());
}

} // namespace

void hud_begin() {
  pinMode(PIN_EPD_CS, OUTPUT);
  pinMode(PIN_EPD_DC, OUTPUT);
  pinMode(PIN_EPD_RST, OUTPUT);
  pinMode(PIN_EPD_BUSY, INPUT);

  SPI.begin(PIN_EPD_SCLK, /*MISO*/ -1, PIN_EPD_MOSI, PIN_EPD_CS);
  display.init(115200, true, 50, false);
  display.setRotation(3);

  Serial.printf("[hud] panel %dx%d rot=3\n", display.width(), display.height());

  Telemetry empty;
  paintFrame(empty, Telemetry::Freshness::Empty, false, /*partial=*/false);
  g_last_full_ms = millis();
  g_last_drawn_ms = g_last_full_ms;
  g_partials_since_full = 0;
}

void hud_update(const Telemetry &tel, uint32_t now_ms, bool ble_linked) {
  const Telemetry::Freshness f = tel.freshness(now_ms);

  const bool new_packet = tel.write_count != g_last_write_seen;
  const bool fresh_changed = f != g_last_fresh;
  const bool link_changed = ble_linked != g_last_ble_linked;
  const bool page_changed = g_page_dirty;

  // Ghost cleanup only after real partial activity — never on idle WAITING.
  const bool due_full =
      g_partials_since_full >= kPartialsBeforeFull &&
      (now_ms - g_last_full_ms) >= kFullRefreshIntervalMs;

  bool dirty = page_changed || fresh_changed || link_changed || due_full;
  if (new_packet) {
    if (g_last_drawn_ms == 0 ||
        (now_ms - g_last_drawn_ms) >= kMinPartialIntervalMs) {
      dirty = true;
    }
  }

  if (!dirty) {
    return;
  }

  const bool force_full = due_full;
  paintFrame(tel, f, ble_linked, /*partial=*/!force_full);

  g_last_drawn_ms = now_ms;
  g_last_write_seen = tel.write_count;
  g_last_fresh = f;
  g_last_ble_linked = ble_linked;
  g_page_dirty = false;
  if (force_full) {
    g_last_full_ms = now_ms;
    g_partials_since_full = 0;
  } else {
    if (g_partials_since_full < 0xFFFF) {
      g_partials_since_full++;
    }
  }
}

void hud_next_page() {
  g_page = (g_page + 1) % kPageCount;
  g_page_dirty = true;
}

void hud_prev_page() {
  g_page = (g_page + kPageCount - 1) % kPageCount;
  g_page_dirty = true;
}

uint8_t hud_current_page() { return g_page; }
