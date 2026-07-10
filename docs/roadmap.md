# Roadmap

## Phase 0 — Desk HUD ✅

- [x] Freeze `protocol/` packet + UUIDs  
- [x] Firmware PlatformIO project, partitions, pins  
- [x] Fake/demo telemetry → partial digit layout  
- [x] NimBLE peripheral accepting writes  
- [x] `pio run -e x4` and `pio run -e x4_demo` link cleanly  
- [x] HUD threads BLE link state onto status page  
- [x] Public GitHub repo + CI (firmware / Swift / iOS simulator)  
- [x] Release workflow (tag `v*`) with firmware artifacts  

## Phase 1 — BLE loop

- [ ] Flash X4, verify free heap > 80 KB with continuous writes  
- [ ] nRF Connect or macOS light tester writes test vector  
- [ ] Stale/weak-link UI behaves on desk  
- [ ] Button page flip works  

## Phase 2 — iOS hub (rideable, no cadence)

- [x] Xcode app scaffold (`ios/BikeHudApp`) + local `BikeHudProtocol`  
- [x] `CBCentralManager` connect to `BikeHUD`, 1 Hz writes  
- [x] Demo ride (desk) + GPS ride (Core Location)  
- [x] Background modes: location + bluetooth-central (plist)  
- [ ] Signing / on-device test with live X4  
- [ ] HealthKit HR  
- [ ] Jersey-pocket ride: phone locked, Watch worn, X4 shows HR + speed  

## Phase 3 — Case & mount

- [ ] Sealed stem / Garmin quarter-turn enclosure  
- [ ] Field ghosting tune (full clear interval)  

## Phase 4 — Cadence

- [ ] Phone CSC Central, fill `cadence_rpm` + flag  
- [ ] Buy CR2032 BLE CSC cadence sensor  

## Phase 5 — Watch hub (optional)

- [ ] watchOS app links `BikeHudProtocol`  
- [ ] Leave-phone mode, single-writer rules in `hubs.md`  

## Non-goals (until hardware changes)

- Multi-sensor BLE central on the X4  
- On-device maps / Strava segments / GPX follow  
- Fork CrossPoint reader into this firmware  
