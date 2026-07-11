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
- [x] Protocol host tests (C) + expanded Swift package tests in CI  
- [x] Contributing guide + issue/PR templates  
 

## Phase 1 — BLE loop

- [x] Flash X4, verify free heap > 80 KB with continuous writes  
- [ ] nRF Connect or macOS light tester writes test vector  
- [x] Stale/weak-link UI behaves on desk  
- [x] Button page flip works (CrossPoint ADC ladder ranges + 11 dB attenuation)  
- [x] Soft-sleep on long power press (no NimBLE deinit crash; wake hold timer fix)  
- [x] Inverted sleep splash + bike silhouette  
- [x] Side/front button map: pages · Confirm pause notify · Back invert  
- [x] Control characteristic `B10E0003` (X4 → hub `PAUSE_TOGGLE`)  
- [x] Auto-sleep idle timer safe vs mid-loop `power_note_activity()`  

## Phase 2 — iOS hub (rideable, no cadence)

- [x] Xcode app scaffold (`ios/BikeHudApp`) + local `BikeHudProtocol`  
- [x] `CBCentralManager` connect to `BikeHUD`, 1 Hz writes  
- [x] Demo ride (desk) + GPS ride (Core Location)  
- [x] Background modes: location + bluetooth-central (plist)  
- [x] Control notify: subscribe + `togglePause()` from HUD Confirm  
- [ ] Rebuild / on-device retest of control + pause with latest package  
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
