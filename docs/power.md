# Power management (X4)

Inspired by CrossPoint’s `HalPowerManager` / `enterDeepSleep`.

## Hardware

| Pin | Role |
|---|---|
| **GPIO3** | Power button (active **low**) |
| **GPIO13** | Battery **latch MOSFET** — LOW cuts MCU power on battery |

CrossPoint drives **GPIO13 low + `gpio_hold_en`** before deep sleep. On battery the MCU is then fully off (including RTC). The power button is hard-wired to briefly power the rail and boot the chip — GPIO wake is mainly for **USB-powered** wake.

E-ink is bistable: the last image (sleep splash) stays with almost no panel power.

## BikeHUD behaviour

### Manual sleep
1. Hold power ≥ **500 ms**  
2. Full-refresh splash: **BikeHUD / Sleeping / hold power to wake**  
3. Wait for button **release**  
4. Enter sleep:
   - **Unplugged:** deep sleep (latch low, GPIO wake armed)  
   - **USB connected:** **soft-sleep** idle loop (USB-JTAG makes deep sleep bounce: black → white → UI)

### Manual wake
- **Battery deep sleep:** press power (hardware power-up → cold boot)  
- **Soft-sleep:** hold power ≥ 500 ms again → BLE + UI resume  

### Auto-sleep (inactivity)
Same idea as CrossPoint’s default **10 minute** timeout:

| Resets the idle timer |
|---|
| Side buttons (page flip, etc.) |
| BLE connect / disconnect |
| Telemetry or time-sync writes |

Constant: `kAutoSleepIdleMs` in `firmware/include/power.h` (default `10 * 60 * 1000`). Set to `0` to disable.

## If wake fails

1. Hold power 1–2 s  
2. Plug USB-C (reset / soft path)  
3. `pio run -e x4 -t upload`  
4. Restore CrossPoint from backup if needed  

## Power budget (rough)

| State | Notes |
|---|---|
| Awake + BLE | Tens of mA (ride) |
| Soft-sleep (USB) | Lower than active UI, not µA |
| Deep sleep + latch (battery) | Near-off; CrossPoint-class |
| E-ink static splash | Negligible |

## Future

- Settings packet from iOS for timeout minutes / never  
- Light sleep while connected  
- Optional “keep awake while LIVE packets flow” is already true via activity notes  
