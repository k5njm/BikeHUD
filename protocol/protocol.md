# Bike HUD BLE protocol

**Telemetry version:** 1  
**Message size:** **16 bytes fixed** (fits default BLE write-without-response)  
**Endianness:** little-endian  
**Role:** Apple hub (Central) → X4 (Peripheral)

## UUIDs

| Entity | UUID |
|---|---|
| Service | `B10E0001-C0C0-41A3-B4C6-42494B454855` |
| Telemetry characteristic (write w/o rsp + read) | `B10E0002-C0C0-41A3-B4C6-42494B454855` |
| Device name advertisement | `BikeHUD` |

## Characteristic usage

- **Properties:** `writeWithoutResponse`, `read` (and `write` as fallback).
- Two **message types** share the same 16-byte characteristic; discriminated by byte 0:
  - `0x01` — telemetry (~1 Hz while a workout is live)
  - `0x10` — wall-clock sync (on connect, then occasionally)
- X4 does not ACK. Telemetry stamps `recv_ms`; time sync sets a free-running software clock.

## Packet `BikeHudPacketV1` — telemetry (version = `1`)

| Offset | Type | Field | Unit / notes |
|---|---|---|---|
| 0 | `u8` | `version` | Must be `1` |
| 1 | `u8` | `flags` | Bitfield (below) |
| 2 | `u16` | `speed_cm_s` | Instantaneous speed, centimetres/second |
| 4 | `u16` | `distance_m` | Ride distance, metres |
| 6 | `u16` | `elapsed_s` | Elapsed workout seconds (DURATION on HUD) |
| 8 | `u8` | `hr_bpm` | Heart rate; valid only if `FLAG_HR_VALID` |
| 9 | `u8` | `cadence_rpm` | RPM; `0xFF` = unknown; valid only if `FLAG_CADENCE_VALID` |
| 10 | `i16` | `elev_m` | Elevation metres |
| 12 | `u8` | `batt_pct` | Hub battery 0–100; `0xFF` = unknown |
| 13 | `u8` | `gps_acc_m` | Horizontal accuracy metres; `0xFF` = unknown |
| 14 | `u8` | `hub_type` | `0` unknown, `1` iPhone, `2` Apple Watch |
| 15 | `u8` | `reserved` | Write `0` |

### Flags (`flags`)

| Bit | Name | Meaning |
|---|---|---|
| 0 | `FLAG_HR_VALID` | `hr_bpm` is trustworthy |
| 1 | `FLAG_CADENCE_VALID` | `cadence_rpm` is trustworthy (and not `0xFF`) |
| 2 | `FLAG_GPS_VALID` | Speed/distance/elev from a fix |
| 3 | `FLAG_PAUSED` | Workout paused |
| 4 | `FLAG_LIVE` | Active live workout session |
| 5–7 | reserved | Must be 0 |

## Packet `BikeHudTimeSync` — wall clock (version = `0x10`)

Still **16 bytes**. Does **not** update ride metrics. HUD stores the instant and free-runs with `millis()` until the next sync (or forever after disconnect — may drift).

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | `u8` | `version` | `0x10` (`BIKE_HUD_MSG_TIME_SYNC`) |
| 1 | `u8` | `flags` | Write `0` |
| 2 | `u16` | `year` | e.g. 2026 |
| 4 | `u8` | `month` | 1–12 |
| 5 | `u8` | `day` | 1–31 |
| 6 | `u8` | `hour` | 0–23 local |
| 7 | `u8` | `minute` | 0–59 |
| 8 | `u8` | `second` | 0–59 |
| 9 | `u8` | `day_of_week` | 0 = Sunday … 6 = Saturday |
| 10–15 | `u8[6]` | reserved | Write `0` |

**Hub policy:** send once when the HUD becomes Ready; refresh every ~5 minutes while connected. Telemetry stays pure 16 B @ 1 Hz.

**Design note:** the HUD is still “dumb” for *ride* data (no sensors). Wall clock is optional chrome that survives brief disconnects; after long power-off it simply disappears until the next sync.

## Stale rules (telemetry only)

| Age since last **telemetry** packet | HUD behaviour |
|---|---|
| ≤ 5 s | Live numbers |
| 5–15 s | Values + “WEAK” |
| \> 15 s | “STALE” — hide live numbers as truth |

HR/cadence **sparklines keep the last samples** across WEAK/STALE so a short gap does not wipe the map; new live samples append again.

Constants: `BIKE_HUD_STALE_WEAK_MS = 5000`, `BIKE_HUD_STALE_HARD_MS = 15000`.

## Display cadence

- Apply telemetry on write; partial e-ink ~1 Hz max.
- Full waveform only after many partials (ghost cleanup).
- Wall clock redraws about once per minute when set.

## Test vector (telemetry v1)

```
01 15 BC 02 68 30 4C 0F 94 FF 38 01 49 05 01 00
```

Canonical C: [`bike_hud_protocol.h`](bike_hud_protocol.h).  
Swift: `ios/BikeHudProtocol`.
