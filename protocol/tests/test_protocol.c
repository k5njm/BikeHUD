/**
 * Host-side protocol smoke tests (no Arduino).
 * Build:  cc -std=c11 -I.. -o test_protocol test_protocol.c && ./test_protocol
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
  EXPECT(sizeof(BikeHudPacketV1) == BIKE_HUD_PACKET_V1_SIZE, "packet size 16");
  EXPECT(BIKE_HUD_PROTOCOL_VERSION == 1u, "version 1");

  /* Canonical test vector from protocol.md */
  const uint8_t hex[16] = {0x01, 0x15, 0xBC, 0x02, 0x68, 0x30, 0x4C, 0x0F,
                           0x94, 0xFF, 0x38, 0x01, 0x49, 0x05, 0x01, 0x00};

  BikeHudPacketV1 p;
  memcpy(&p, hex, sizeof(p));

  EXPECT(bike_hud_packet_version_ok(&p), "version ok");
  EXPECT(p.flags == 0x15, "flags LIVE|GPS|HR");
  EXPECT(p.speed_cm_s == 700, "speed 700 cm/s");
  EXPECT(p.distance_m == 0x3068, "distance");
  EXPECT(p.elapsed_s == 0x0F4C, "elapsed");
  EXPECT(p.hr_bpm == 0x94, "hr");
  EXPECT(p.cadence_rpm == BIKE_HUD_UNKNOWN_U8, "cadence unknown");
  EXPECT(p.elev_m == 312, "elev");
  EXPECT(p.batt_pct == 0x49, "batt");
  EXPECT(p.gps_acc_m == 5, "gps");
  EXPECT(p.hub_type == BIKE_HUD_HUB_IPHONE, "hub iPhone");
  EXPECT(p.reserved == 0, "reserved");

  /* 700 cm/s → 25.2 km/h → 252 as x10 */
  EXPECT(bike_hud_speed_kmh_x10(700) == 252, "speed km/h x10");

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
