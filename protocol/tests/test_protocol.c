/**
 * Host-side protocol smoke tests (no Arduino).
 * Build:  cc -std=c11 -I protocol -o test_protocol protocol/tests/test_protocol.c
 */
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
  EXPECT(sizeof(BikeHudPacketV1) == BIKE_HUD_PACKET_V1_SIZE, "v1 size 16");
  EXPECT(sizeof(BikeHudPacketV2) == BIKE_HUD_PACKET_V2_SIZE, "v2 size 24");
  EXPECT(BIKE_HUD_PROTOCOL_VERSION == 2u, "preferred version 2");

  const uint8_t hex[16] = {0x01, 0x15, 0xBC, 0x02, 0x68, 0x30, 0x4C, 0x0F,
                           0x94, 0xFF, 0x38, 0x01, 0x49, 0x05, 0x01, 0x00};

  BikeHudPacketV1 p;
  memcpy(&p, hex, sizeof(p));

  EXPECT(bike_hud_packet_version_ok(&p), "v1 version ok");
  EXPECT(p.flags == 0x15, "flags LIVE|GPS|HR");
  EXPECT(p.speed_cm_s == 700, "speed 700 cm/s");
  EXPECT(p.distance_m == 0x3068, "distance");
  EXPECT(p.elapsed_s == 0x0F4C, "elapsed");
  EXPECT(p.hr_bpm == 0x94, "hr");
  EXPECT(p.cadence_rpm == BIKE_HUD_UNKNOWN_U8, "cadence unknown");
  EXPECT(p.elev_m == 312, "elev");
  EXPECT(bike_hud_speed_kmh_x10(700) == 252, "speed km/h x10");

  BikeHudPacketV2 v2;
  memset(&v2, 0, sizeof(v2));
  v2.body = p;
  v2.body.version = BIKE_HUD_PROTOCOL_VERSION_V2;
  v2.body.flags = (uint8_t)(p.flags | BIKE_HUD_FLAG_CLOCK_VALID);
  v2.year = 2026;
  v2.month = 7;
  v2.day = 10;
  v2.hour = 18;
  v2.minute = 34;
  v2.second = 0;
  v2.day_of_week = 5; /* Friday */
  EXPECT(bike_hud_packet_version_ok(&v2.body), "v2 version ok");
  EXPECT(bike_hud_clock_valid(&v2.body), "clock valid flag");

  BikeHudPacketV1 bad = p;
  bad.version = 99;
  EXPECT(!bike_hud_packet_version_ok(&bad), "reject bad version");

  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("protocol host tests OK\n");
  return 0;
}
