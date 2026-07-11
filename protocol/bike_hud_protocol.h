/**
 * Bike HUD wire protocol — shared between X4 firmware (C) and Apple apps.
 * Pure C, no Arduino.
 *
 * All messages are **16 bytes** on the telemetry characteristic (fits default
 * BLE write-without-response). Telemetry is version 1. Wall clock is a
 * separate message type written occasionally; the X4 free-runs a software
 * clock between syncs.
 */
#ifndef BIKE_HUD_PROTOCOL_H
#define BIKE_HUD_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Telemetry metrics packet. */
#define BIKE_HUD_PROTOCOL_VERSION 1u
#define BIKE_HUD_PACKET_V1_SIZE 16u

/**
 * Time-sync message (same 16-byte slot, different version byte).
 * Does not replace ride metrics — only sets the HUD wall clock.
 */
#define BIKE_HUD_MSG_TIME_SYNC 0x10u

/** Advertised BLE local name. */
#define BIKE_HUD_DEVICE_NAME "BikeHUD"

#define BIKE_HUD_UUID_SERVICE_STR "B10E0001-C0C0-41A3-B4C6-42494B454855"
#define BIKE_HUD_UUID_TELEMETRY_STR "B10E0002-C0C0-41A3-B4C6-42494B454855"

/** flags bitfield (telemetry) */
#define BIKE_HUD_FLAG_HR_VALID (1u << 0)
#define BIKE_HUD_FLAG_CADENCE_VALID (1u << 1)
#define BIKE_HUD_FLAG_GPS_VALID (1u << 2)
#define BIKE_HUD_FLAG_PAUSED (1u << 3)
#define BIKE_HUD_FLAG_LIVE (1u << 4)

#define BIKE_HUD_UNKNOWN_U8 0xFFu

#define BIKE_HUD_HUB_UNKNOWN 0u
#define BIKE_HUD_HUB_IPHONE 1u
#define BIKE_HUD_HUB_WATCH 2u

#define BIKE_HUD_STALE_WEAK_MS 5000u
#define BIKE_HUD_STALE_HARD_MS 15000u

#pragma pack(push, 1)
typedef struct BikeHudPacketV1 {
  uint8_t version; /* BIKE_HUD_PROTOCOL_VERSION (1) */
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
 * Time sync — still 16 bytes on the same characteristic.
 * day_of_week: 0 = Sunday … 6 = Saturday.
 * Local time (hub already applied timezone).
 */
typedef struct BikeHudTimeSync {
  uint8_t version; /* BIKE_HUD_MSG_TIME_SYNC (0x10) */
  uint8_t flags;   /* write 0 */
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t day_of_week;
  uint8_t reserved[6];
} BikeHudTimeSync;
#pragma pack(pop)

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(BikeHudPacketV1) == BIKE_HUD_PACKET_V1_SIZE,
               "BikeHudPacketV1 must be 16 bytes");
_Static_assert(sizeof(BikeHudTimeSync) == BIKE_HUD_PACKET_V1_SIZE,
               "BikeHudTimeSync must be 16 bytes");
#endif

static inline uint16_t bike_hud_speed_kmh_x10(uint16_t speed_cm_s) {
  return (uint16_t)(((uint32_t)speed_cm_s * 36u) / 100u);
}

static inline int bike_hud_is_telemetry(const uint8_t *raw, uint16_t len) {
  return raw && len == BIKE_HUD_PACKET_V1_SIZE &&
         raw[0] == BIKE_HUD_PROTOCOL_VERSION;
}

static inline int bike_hud_is_time_sync(const uint8_t *raw, uint16_t len) {
  return raw && len == BIKE_HUD_PACKET_V1_SIZE &&
         raw[0] == BIKE_HUD_MSG_TIME_SYNC;
}

static inline int bike_hud_packet_version_ok(const BikeHudPacketV1 *p) {
  return p && p->version == BIKE_HUD_PROTOCOL_VERSION;
}

#ifdef __cplusplus
}
#endif

#endif /* BIKE_HUD_PROTOCOL_H */
