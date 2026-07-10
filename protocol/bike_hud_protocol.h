/**
 * Bike HUD wire protocol — shared between X4 firmware (C) and Apple apps (via
 * matching Swift layout in ios/BikeHudProtocol).
 *
 * Keep this header pure C, no Arduino, so host tools can include it too.
 */
#ifndef BIKE_HUD_PROTOCOL_H
#define BIKE_HUD_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Preferred version for new writers (includes wall clock trailer). */
#define BIKE_HUD_PROTOCOL_VERSION 2u
#define BIKE_HUD_PROTOCOL_VERSION_V1 1u
#define BIKE_HUD_PROTOCOL_VERSION_V2 2u

/** Fixed packet sizes. */
#define BIKE_HUD_PACKET_V1_SIZE 16u
#define BIKE_HUD_PACKET_V2_SIZE 24u

/** Advertised BLE local name. */
#define BIKE_HUD_DEVICE_NAME "BikeHUD"

/**
 * String forms (CoreBluetooth / nRF Connect / NimBLE):
 *   Service: B10E0001-C0C0-41A3-B4C6-42494B454855
 *   Char:    B10E0002-C0C0-41A3-B4C6-42494B454855
 */
#define BIKE_HUD_UUID_SERVICE_STR "B10E0001-C0C0-41A3-B4C6-42494B454855"
#define BIKE_HUD_UUID_TELEMETRY_STR "B10E0002-C0C0-41A3-B4C6-42494B454855"

/** flags bitfield */
#define BIKE_HUD_FLAG_HR_VALID (1u << 0)
#define BIKE_HUD_FLAG_CADENCE_VALID (1u << 1)
#define BIKE_HUD_FLAG_GPS_VALID (1u << 2)
#define BIKE_HUD_FLAG_PAUSED (1u << 3)
#define BIKE_HUD_FLAG_LIVE (1u << 4)
/** Hub filled wall-clock fields (v2 trailer valid). */
#define BIKE_HUD_FLAG_CLOCK_VALID (1u << 5)

/** cadence_rpm / batt / gps_acc sentinel */
#define BIKE_HUD_UNKNOWN_U8 0xFFu

/** hub_type */
#define BIKE_HUD_HUB_UNKNOWN 0u
#define BIKE_HUD_HUB_IPHONE 1u
#define BIKE_HUD_HUB_WATCH 2u

/** Stale thresholds on the X4 (ms since last good packet). Not RF RSSI. */
#define BIKE_HUD_STALE_WEAK_MS 5000u
#define BIKE_HUD_STALE_HARD_MS 15000u

#pragma pack(push, 1)
/** Core metrics — v1 body (also first 16 bytes of v2). */
typedef struct BikeHudPacketV1 {
  uint8_t version;
  uint8_t flags;
  uint16_t speed_cm_s;
  uint16_t distance_m;
  uint16_t elapsed_s;
  uint8_t hr_bpm;
  uint8_t cadence_rpm;
  int16_t elev_m;
  uint8_t batt_pct;
  uint8_t gps_acc_m;
  uint8_t hub_type;
  uint8_t reserved;
} BikeHudPacketV1;

/**
 * v2 = v1 body + local wall clock from the hub (timezone already applied).
 * day_of_week: 0 = Sunday … 6 = Saturday (matches many libc conventions).
 */
typedef struct BikeHudPacketV2 {
  BikeHudPacketV1 body;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t day_of_week;
} BikeHudPacketV2;
#pragma pack(pop)

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(BikeHudPacketV1) == BIKE_HUD_PACKET_V1_SIZE,
               "BikeHudPacketV1 must be 16 bytes");
_Static_assert(sizeof(BikeHudPacketV2) == BIKE_HUD_PACKET_V2_SIZE,
               "BikeHudPacketV2 must be 24 bytes");
#endif

/** Convert cm/s to km/h (integer, truncated) * 10 for one decimal. */
static inline uint16_t bike_hud_speed_kmh_x10(uint16_t speed_cm_s) {
  return (uint16_t)(((uint32_t)speed_cm_s * 36u) / 100u);
}

/** True if version is a known major packet format. */
static inline int bike_hud_packet_version_ok(const BikeHudPacketV1 *p) {
  return p && (p->version == BIKE_HUD_PROTOCOL_VERSION_V1 ||
               p->version == BIKE_HUD_PROTOCOL_VERSION_V2);
}

static inline int bike_hud_clock_valid(const BikeHudPacketV1 *p) {
  return p && (p->flags & BIKE_HUD_FLAG_CLOCK_VALID) &&
         p->version == BIKE_HUD_PROTOCOL_VERSION_V2;
}

#ifdef __cplusplus
}
#endif

#endif /* BIKE_HUD_PROTOCOL_H */
