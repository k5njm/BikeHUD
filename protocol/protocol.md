# Bike HUD BLE protocol

**Version:** 1  
**Endianness:** little-endian  
**Packet size:** 16 bytes, fixed  
**Role:** Apple hub (Central) → X4 (Peripheral)

## UUIDs

| Entity | UUID |
|---|---|
| Service | `B10E0001-C0C0-41A3-B4C6-42494B454855` |
| Telemetry characteristic (write w/o rsp + read) | `B10E0002-C0C0-41A3-B4C6-42494B454855` |
| Device name advertisement | `BikeHUD` |

> `B10E…42494B454855` is valid hex (`42494B45` = ASCII `BIKE`). Randomize fully only if a collision ever shows up in the wild.

## Characteristic usage

- **Properties:** `writeWithoutResponse`, `read` (optional `notify` later for X4→hub status).
- Phone/Watch **writes** one full 16-byte `BikeHudPacketV1` about **1 Hz** while a workout is live.
- X4 stamps `recv_ms` on every successful parse; does not ACK.

## Packet `BikeHudPacketV1` (16 bytes)

| Offset | Type | Field | Unit / notes |
|---|---|---|---|
| 0 | `u8` | `version` | Must be `1` |
| 1 | `u8` | `flags` | Bitfield (below) |
| 2 | `u16` | `speed_cm_s` | Instantaneous speed, centimetres/second |
| 4 | `u16` | `distance_m` | Ride distance, metres |
| 6 | `u16` | `elapsed_s` | Elapsed workout seconds (includes pauses if you want wall clock; prefer moving time and set paused flag) |
| 8 | `u8` | `hr_bpm` | Heart rate; valid only if `FLAG_HR_VALID` |
| 9 | `u8` | `cadence_rpm` | RPM; `0xFF` = unknown; valid only if `FLAG_CADENCE_VALID` |
| 10 | `i16` | `elev_m` | Elevation metres (GPS or baro on hub) |
| 12 | `u8` | `batt_pct` | Hub battery 0–100; `0xFF` = unknown |
| 13 | `u8` | `gps_acc_m` | Horizontal accuracy metres; `0xFF` = unknown |
| 14 | `u8` | `hub_type` | `0` unknown, `1` iPhone, `2` Apple Watch |
| 15 | `u8` | `reserved` | Write `0` |

Canonical C layout: [`bike_hud_protocol.h`](bike_hud_protocol.h).  
Swift mirror: `ios/BikeHudProtocol`.

### Flags (`flags`)

| Bit | Name | Meaning |
|---|---|---|
| 0 | `FLAG_HR_VALID` | `hr_bpm` is trustworthy |
| 1 | `FLAG_CADENCE_VALID` | `cadence_rpm` is trustworthy (and not `0xFF`) |
| 2 | `FLAG_GPS_VALID` | Speed/distance/elev from a fix, not dead-reckoning only |
| 3 | `FLAG_PAUSED` | Workout paused — HUD may show pause chrome |
| 4 | `FLAG_LIVE` | Active live workout session (always set when streaming) |
| 5–7 | reserved | Must be 0 in v1 |

## Stale rules (X4)

Inspired by stravaV10 locator age.

| Age since last good packet | HUD behaviour |
|---|---|
| ≤ 3 s | Live numbers |
| 3–8 s | Show values + “WEAK LINK” status |
| \> 8 s | “NO DATA” / waiting screen — **do not** present last numbers as truth |

Constants: `BIKE_HUD_STALE_WEAK_MS = 3000`, `BIKE_HUD_STALE_HARD_MS = 8000`.

## Display cadence

- Apply packet immediately on write when fields change.
- Partial e-ink refresh of digit boxes ~1 Hz max.
- Full waveform clear every ~45–60 s (ghost cleanup).
- Page flip on X4 buttons (left/right); not pushed from hub in v1.

## Hub duties (Apple)

1. Single writer only (phone **or** Watch) — see [`docs/hubs.md`](../docs/hubs.md).
2. Own CSC cadence math, HealthKit HR, Core Location speed/distance.
3. Keep `version == 1` and set validity flags honestly (no fake zeros for missing HR).
4. Target ~1 Hz writes; never burst multi-kHz.

## Versioning

- Bump `version` only when size or field semantics break readers.
- X4 ignores packets with unknown `version` (log + keep last good).
- Additive fields require v2+ and a negotiated size, not silent extension past 16 bytes.

## Test vector (v1)

All LE. Live phone hub, 25.2 km/h, 12.4 km, 1 h 5 m, HR 148, no cadence, elev 312 m, 73% batt, 5 m GPS chrome.

```
version     = 01
flags       = 15   # LIVE|GPS|HR = 0x10|0x04|0x01
speed_cm_s  = BC 06   # 1724 cm/s ≈ 62.06 km/h wait — use 700 cm/s for 25.2 km/h:
// Correct: 25.2 km/h = 25.2 * 1000/36 = 700 cm/s → 0x02BC
distance_m  = 94 30   # 12436? use 12400 m → 0x3068
elapsed_s   = 4C 0F   # 3916 s ≈ 1h05m
hr_bpm      = 94
cadence_rpm = FF
elev_m      = 38 01   # 312
batt_pct    = 49
gps_acc_m   = 05
hub_type    = 01
reserved    = 00
```

Canonical hex dump for automated tests:

```
01 15 BC 02 68 30 4C 0F 94 FF 38 01 49 05 01 00
```

- speed = 0x02BC = 700 cm/s → 25.2 km/h  
- distance = 0x3068 = 12392 m ≈ 12.4 km  
- elapsed = 0x0F4C = 3916 s  
