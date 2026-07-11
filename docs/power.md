# Power management (X4)

## Hardware vs software

On the Xteink X4 the **top power button is GPIO3 (active low)**. There is no
separate “always-off” power cut that CrossPoint exclusively owns — **sleep and
wake are implemented in software**, same as CrossPoint:

| Action | Behaviour |
|---|---|
| **Hold power ~0.5 s (awake)** | Draw sleep splash → stop BLE → **deep sleep** |
| **Press power (asleep)** | ESP32-C3 **GPIO wake** → full reboot into BikeHUD |
| **E-ink panel** | Keeps the last image with almost no power while MCU sleeps |

So a “frozen” screen is normal when sleeping — the panel is bistable. The MCU is
mostly off; it is **not** stuck mid-frame unless something else is wrong.

## BikeHUD behaviour

1. Long-press power ≥ 500 ms  
2. Splash: **BikeHUD / Sleeping / hold power to wake**  
3. Wait for button **release** (so wake is not instant)  
4. `esp_deep_sleep` with wake on GPIO3 low  
5. Next power press → cold boot → waiting / BLE as usual  

Side buttons (page left/right) do not sleep the device.

## If it will not wake

1. Hold the power button 1–2 s.  
2. Plug **USB-C** (data cable) — USB reset often brings the chip back.  
3. Flash again: `cd firmware && pio run -e x4 -t upload`  
4. Restore CrossPoint from backup if needed (`docs/flash-and-restore.md`).

Some units need a firm press on the edge power key; it is easy to short-tap.

## Power budget (rough)

| State | Notes |
|---|---|
| Awake + BLE advertising / connected | Tens of mA (dominates ride day) |
| Deep sleep + e-ink static image | µA class on the ESP32-C3 path |
| Partial e-ink refresh | Short spikes when painting |

For multi-day “leave on the bars” use **sleep** between rides. For an active ride
leave the unit awake so BLE stays connected.

## Future ideas

- Auto-sleep after N minutes with no BLE / no packets  
- Lighter light-sleep while connected (harder with NimBLE + e-ink)  
- Richer sleep art / battery % on splash  
