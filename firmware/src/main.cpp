/**
 * XTEink X4 Bike HUD
 *
 * Peripheral BLE telemetry display. Hub (iPhone / later Watch) owns sensors.
 */

#include <Arduino.h>

#include "bike_hud_protocol.h"
#include "ble_service.h"
#include "buttons.h"
#include "hud.h"
#include "telemetry.h"

Telemetry g_telemetry;

#if defined(BIKE_HUD_DEMO)
namespace {
void demo_tick(uint32_t now_ms) {
  static uint32_t last = 0;
  if (now_ms - last < 1000) {
    return;
  }
  last = now_ms;

  // Walk through a synthetic ride so the panel can be tested without a phone.
  static uint16_t elapsed = 0;
  static uint16_t dist = 0;
  elapsed++;
  dist = (uint16_t)(elapsed * 7); // ~25 km/h → 7 m/s

  BikeHudPacketV1 p{};
  p.version = BIKE_HUD_PROTOCOL_VERSION;
  p.flags = BIKE_HUD_FLAG_LIVE | BIKE_HUD_FLAG_HR_VALID | BIKE_HUD_FLAG_GPS_VALID;
  p.speed_cm_s = 700; // 25.2 km/h
  p.distance_m = dist;
  p.elapsed_s = elapsed;
  p.hr_bpm = (uint8_t)(120 + (elapsed % 40));
  p.cadence_rpm = BIKE_HUD_UNKNOWN_U8;
  p.elev_m = 300 + (int16_t)(elapsed % 50);
  p.batt_pct = 80;
  p.gps_acc_m = 5;
  p.hub_type = BIKE_HUD_HUB_IPHONE;
  p.reserved = 0;

  g_telemetry.apply(p, now_ms);
}
} // namespace
#endif

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== BikeHUD X4 ===");
  Serial.printf("protocol v%u  packet %u B (time sync msg 0x%02x)\n",
                BIKE_HUD_PROTOCOL_VERSION, (unsigned)sizeof(BikeHudPacketV1),
                BIKE_HUD_MSG_TIME_SYNC);
  Serial.printf("free heap boot: %u\n", (unsigned)ESP.getFreeHeap());

  buttons_begin();
  hud_begin();

#if defined(BIKE_HUD_DEMO)
  Serial.println("[main] DEMO mode — synthetic telemetry, BLE off");
#else
  ble_service_begin();
#endif

  Serial.printf("free heap ready: %u\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
  const uint32_t now = millis();

#if defined(BIKE_HUD_DEMO)
  demo_tick(now);
#else
  ble_service_loop();
#endif

  switch (buttons_poll()) {
  case BoardButton::Right:
  case BoardButton::VolumeDown:
    hud_next_page();
    break;
  case BoardButton::Left:
  case BoardButton::VolumeUp:
    hud_prev_page();
    break;
  default:
    break;
  }

#if defined(BIKE_HUD_DEMO)
  hud_update(g_telemetry, now, /*ble_linked=*/false);
#else
  hud_update(g_telemetry, now, ble_service_is_connected());
#endif

  static uint32_t last_heap_log = 0;
  if (now - last_heap_log > 5000) {
    last_heap_log = now;
    Serial.printf("[mem] free=%u min=%u connected=%d pkts=%lu page=%u\n",
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMinFreeHeap(),
#if defined(BIKE_HUD_DEMO)
                  0,
#else
                  (int)ble_service_is_connected(),
#endif
                  (unsigned long)g_telemetry.write_count, hud_current_page());
  }

  delay(10);
}
