# Hubs (phone vs Watch)

The X4 does not care who writes packets. The Apple side must ensure **exactly one active writer**.

## Roles

| Hub | When to use |
|---|---|
| **iPhone (R1 default)** | Maps, music, easy CSC pairing, longer battery as platform |
| **Cellular Watch (R2)** | Leave-phone outdoor cycle days |
| Dual | Never dual-write to the HUD |

## Rules

1. **Start workout before feeding HUD.** watchOS/iOS background kills free-floating BLE without a live workout session.
2. **Phone present + ride started from phone app** → phone is writer; Watch may still supply HR via system workout mirror.
3. **Phone absent / explicit “Watch mode”** → Watch owns session, location, optional CSC, and CBCentral → X4.
4. If both apps think they own the ride, **prefer phone**; Watch shows “HUD claimed by iPhone”.
5. `hub_type` in the packet must match the actual writer (`1` phone, `2` Watch).

## Cadence

- Pair CSC sensors to the **active hub** (phone for R1, Watch for R2).
- Do **not** pair cadence to the X4.
- Cadence-only or dual CSC+speed BLE sensors; avoid ANT+-only units.

## Shared code

```
ios/BikeHudProtocol   // packet encode/decode + UUIDs
// linked into Phone target and (later) Watch target
```

Firmware stays identical across hubs.
