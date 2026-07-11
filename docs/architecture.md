# Architecture

## Topology

```
┌──────────────── Apple hub (single writer) ────────────────┐
│  Workout session · HealthKit HR · Core Location · CSC     │
│                     bikes_hud_protocol v1 packet ~1 Hz    │
└───────────────────────────┬───────────────────────────────┘
                            │ BLE GATT write
                            ▼
┌──────────────── XTEink X4 (ESP32-C3) ─────────────────────┐
│  NimBLE peripheral · parse 16 B · partial e-ink HUD       │
│  Buttons: page flip · Status: live / weak / no data       │
└───────────────────────────────────────────────────────────┘
```

X4 does **not**:

- scan for cadence / HR sensors
- run Wi‑Fi on rides
- layout maps, segments, or EPUB
- record FIT/GPX (hub owns the workout record)

## Why this topology

X4 = ESP32-C3, **no PSRAM**. Multi-central BLE + fonts + full-frame double buffers already fight for ~190 KB free heap. Phone/Watch already own Hear Rate, GNSS, multi-device BLE, background workout priority, and logging.

## Soft layers

```
protocol/          pure C + docs (single source of truth for the wire)
firmware/src       ble_service · telemetry model · hud render · board
ios/BikeHudProtocol  Swift Layout mirror of the C struct
ios app (Xcode)    workout + CBCentralManager + optional CSC
watchOS later      same package, same characteristic write
```

## Display model (stravaV10-inspired)

### Pages (X4 left/right)

| Page | Contents |
|---|---|
| 0 | Speed · HR · Cadence · Elapsed |
| 1 | Distance · Avg speed · Elev · Hub battery |
| 2 | Link status · GPS accuracy · Hub type · free heap (dev) |

### Side / front buttons (current)

Decode uses CrossPoint `InputManager` **open ADC ranges** + `analogSetAttenuation(ADC_11db)` (not fixed ±midpoint windows).

| Physical | Hardware name | Action |
|---|---|---|
| Bottom ↑ / side vol ↑ | Left / VolUp | Previous page |
| Bottom ↓ / side vol ↓ | Right / VolDown | Next page |
| Bottom Confirm | Confirm | Notify hub `PAUSE_TOGGLE` (phone owns workout clock / recording) |
| Bottom Back | Back | Invert ride UI (local full-refresh; black paper / white ink) |
| Power (long hold) | Power | Soft-sleep splash |

Control path: GATT notify on `B10E0003…` (`BikeHudControlEvent`, msg `0x20`).

### Freshness

- weak link after 3 s without a good write  
- hard stale after 8 s → hide live numbers  

## Memory budget (firmware target)

| Item | Aim |
|---|---|
| NimBLE peripheral | ~35–50 KB |
| 1bpp framebuffer region / strips | ≤ 48 KB if full; prefer partial redraws |
| App + fonts | ≤ 30 KB |
| Free heap steady | **> 80 KB** under continuous 1 Hz writes |

Log `ESP.getFreeHeap()` every 5 s. If free heap drops under ~60 KB solve before adding features.

## Power

- e-ink heavy only on refresh  
- between packets: light idle, BLE connected  
- optional deep sleep only when no hub for N minutes (post-MVP)  
- 650 mAh pack should cover typical road rides; measure  

## Ruggedization (product, not code)

Custom sealed stem enclosure + Garmin-style quarter-turn. Stock X4 is not rainproof.

## References

- FreeInk `XTEINK_X4` board profile — pins SPI 8/10/21/4/5/6  
- CrossPoint `partitions.csv` — dual OTA  
- stravaV10 `Sensor` age + VueCRS pages  
