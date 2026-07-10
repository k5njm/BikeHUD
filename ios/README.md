# iOS / watchOS (Apple hub)

Phone app streams ride metrics to the X4 over BLE (~1 Hz `BikeHudPacketV1` writes).

## Layout

| Path | Purpose |
|---|---|
| `BikeHudProtocol/` | Swift package: encode/decode + UUIDs (shared) |
| `BikeHudApp/` | iOS 16+ SwiftUI app (CBCentral + demo/GPS ride) |

## Open & run (needs full Xcode)

This machine may only have Command Line Tools; **use a Mac with Xcode** to install on a physical iPhone (BLE does not work usefully in Simulator for the X4).

```bash
open ios/BikeHudApp/BikeHudApp.xcodeproj
```

1. Select the **BikeHudApp** target → **Signing & Capabilities** → choose your **Team** (fixes empty `DEVELOPMENT_TEAM`).
2. Plug in an iPhone, select it as the run destination.
3. Run (▶). Allow **Bluetooth** when prompted.
4. Power the X4 with BikeHUD firmware (status `WAITING · ADV`).
5. App should show **Ready · BikeHUD** after connect.
6. Mode **Demo ride** → **Start** — X4 digits should update ~1 Hz (≈15.5 mph, HR, cadence, distance).

### GPS ride

- Pick **GPS ride**, grant location, go outside (or near a window).
- Speed/distance/elev from Core Location; HR/cadence still empty until HealthKit / CSC (later).

### Background

`Info.plist` already declares:

- `bluetooth-central`
- `location`

For long pocket rides you may still want **Always** location later; v1 uses When In Use + background location mode.

## Protocol package only

```bash
cd ios/BikeHudProtocol
swift build
# swift test   # needs full Xcode (XCTest)
```

## Architecture (app)

```
RideController          1 Hz timer, builds BikeHudPacketV1
├── HudBleClient        CBCentralManager → writeWithoutResponse
├── LocationTelemetry   CLLocationManager (GPS mode)
└── Demo synthesizer    fixed ~15.5 mph ride for desk tests
```

Wire units stay **metric** on the BLE packet; the X4 firmware renders **imperial** by default.

## Next (not in this scaffold)

- [ ] HealthKit HR (Watch → Phone → X4)
- [ ] BLE CSC cadence sensor on the phone
- [ ] Settings: units override, reconnect to last HUD
- [ ] watchOS hub target (same package, single-writer rules)

## Soft links

- Wire format: [`../protocol/protocol.md`](../protocol/protocol.md)
- Hubs: [`../docs/hubs.md`](../docs/hubs.md)
