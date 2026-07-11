#include "bike_hud_protocol.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define EXPECT(cond, msg)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "FAIL: %s\n", msg);                                      \
      failures++;                                                              \
    }                                                                          \
  } while (0)

int main(void) {
  EXPECT(sizeof(BikeHudPacketV1) == 16, "telemetry 16");
  EXPECT(sizeof(BikeHudTimeSync) == 16, "time sync 16");
  EXPECT(BIKE_HUD_PROTOCOL_VERSION == 1u, "telemetry version 1");
  EXPECT(BIKE_HUD_MSG_TIME_SYNC == 0x10u, "time sync type");

  const uint8_t hex[16] = {0x01, 0x15, 0xBC, 0x02, 0x68, 0x30, 0x4C, 0x0F,
                           0x94, 0xFF, 0x38, 0x01, 0x49, 0x05, 0x01, 0x00};
  BikeHudPacketV1 p;
  memcpy(&p, hex, sizeof(p));
  EXPECT(bike_hud_packet_version_ok(&p), "v1 ok");
  EXPECT(bike_hud_is_telemetry(hex, 16), "is telemetry");
  EXPECT(!bike_hud_is_time_sync(hex, 16), "not time sync");
  EXPECT(bike_hud_speed_kmh_x10(700) == 252, "speed x10");

  BikeHudTimeSync ts = {0};
  ts.version = BIKE_HUD_MSG_TIME_SYNC;
  ts.year = 2026;
  ts.month = 7;
  ts.day = 11;
  ts.hour = 18;
  ts.minute = 34;
  ts.second = 0;
  ts.day_of_week = 5;
  EXPECT(bike_hud_is_time_sync((const uint8_t *)&ts, 16), "is time sync");
  EXPECT(!bike_hud_is_telemetry((const uint8_t *)&ts, 16), "not telemetry");

  EXPECT(sizeof(BikeHudControlEvent) == 16, "control 16");
  EXPECT(BIKE_HUD_MSG_CONTROL == 0x20u, "control type");
  EXPECT(BIKE_HUD_EVT_PAUSE_TOGGLE == 1u, "pause evt");
  BikeHudControlEvent ce = {0};
  ce.version = BIKE_HUD_MSG_CONTROL;
  ce.event = BIKE_HUD_EVT_PAUSE_TOGGLE;
  EXPECT(bike_hud_is_control((const uint8_t *)&ce, 16), "is control");
  EXPECT(!bike_hud_is_telemetry((const uint8_t *)&ce, 16), "control not telem");

  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("protocol host tests OK\n");
  return 0;
}
