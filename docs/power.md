# Power management (X4)

Inspired by CrossPoint’s `HalPowerManager` / `enterDeepSleep`.

## Hardware

| Pin | Role |
|---|---|
| **GPIO3** | Power button (active **low**) |
| **GPIO13** | Battery **latch MOSFET** — LOW cuts MCU power on battery (CrossPoint) |

CrossPoint drives GPIO13 low + hold before deep sleep. On battery the MCU is then fully off; the power button hard-wires power to boot.

## BikeHUD behaviour (current)

### Manual sleep — **soft-sleep only**
1. Hold power ≥ **500 ms**  
2. Splash full-refresh: **BikeHUD / Sleeping / hold power to wake**  
3. **Release** the button completely  
4. Device stays on splash (MCU idle, BLE off)  
5. Hold power ≥ **500 ms** again to wake → BLE + UI  

Deep sleep was bouncing (black → white → ride UI) before the splash could stick, especially with USB-Serial/JTAG. Soft-sleep is intentional until battery deep-sleep is revalidated.

### Auto-sleep
After **10 minutes** with no:
- side buttons  
- BLE link change  
- telemetry / time-sync packets  

Constant: `kAutoSleepIdleMs` in `firmware/include/power.h` (`0` = off).

## CrossPoint deep sleep (planned)

| Step | CrossPoint |
|---|---|
| Wait for power release | yes |
| Tear down radio / serial | yes |
| GPIO13 latch low + hold | yes |
| `esp_deep_sleep` + GPIO wake | yes (USB wake; battery is hard power-on) |

We’ll re-enable that path behind a clear “battery deep sleep” once soft-sleep UX is solid unplugged.

## Debug

Serial (115200) on long-press:

```
[power] === SLEEP BEGIN ===
[power] stopping BLE
[power] drawing splash
[power] splash done
[power] wait for release
[power] armed — hold power to wake
```

If you never see `SLEEP BEGIN`, the hold detector isn’t firing. If you see splash then immediately `AWAKE`, the wake path accepted a press too early.

## If wake fails

1. Hold power 1–2 s  
2. Plug USB-C  
3. `pio run -e x4 -t upload`  
