/**
 * XTEink X4 Bike HUD
 *
 * Peripheral BLE telemetry display. Hub (iPhone / later Watch) owns sensors.
 */

#include <Arduino.h>
#include <esp_sleep.h>

#include "bike_hud_protocol.h"
#include "ble_service.h"
#include "board_pins.h"
#include "buttons.h"
#include "hud.h"
#include "power.h"
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

  // Ensure battery latch is ON after soft-wake / boot (CrossPoint GPIO13).
  pinMode(PIN_PWR_LATCH, OUTPUT);
  digitalWrite(PIN_PWR_LATCH, HIGH);

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.println("[power] woke from deep sleep (GPIO / power button)");
  } else {
    Serial.printf("[power] boot cause %d\n", (int)cause);
  }

  buttons_begin();
  hud_begin();

#if defined(BIKE_HUD_DEMO)
  Serial.println("[main] DEMO mode — synthetic telemetry, BLE off");
#else
  ble_service_begin();
#endif

  power_note_activity();
  Serial.printf("free heap ready: %u\n", (unsigned)ESP.getFreeHeap());
  Serial.printf("[power] auto-sleep after %lu min idle\n",
                (unsigned long)(kAutoSleepIdleMs / 60000UL));
}

void loop() {
  const uint32_t now = millis();

#if defined(BIKE_HUD_DEMO)
  demo_tick(now);
#else
  ble_service_loop();
#endif

  // CrossPoint-style long-press power → sleep.
  if (buttons_power_held_ms() >= kPowerSleepHoldMs) {
    power_enter_sleep();
  }

  static uint32_t last_pkts = 0;
  static bool last_linked = false;
#if !defined(BIKE_HUD_DEMO)
  const bool linked = ble_service_is_connected();
  if (linked != last_linked) {
    last_linked = linked;
    power_note_activity();
  }
  if (g_telemetry.write_count != last_pkts) {
    last_pkts = g_telemetry.write_count;
    power_note_activity(); // BLE telemetry / time-sync counts as activity
  }
#endif

  switch (buttons_poll()) {
  case BoardButton::Right:
  case BoardButton::VolumeDown:
    power_note_activity();
    hud_next_page();
    break;
  case BoardButton::Left:
  case BoardButton::VolumeUp:
    power_note_activity();
    hud_prev_page();
    break;
  case BoardButton::Confirm:
  case BoardButton::Back:
    power_note_activity();
    break;
  default:
    break;
  }

#if defined(BIKE_HUD_DEMO)
  hud_update(g_telemetry, now, /*ble_linked=*/false);
#else
  hud_update(g_telemetry, now, ble_service_is_connected());
#endif

  power_poll_auto_sleep(now);

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
