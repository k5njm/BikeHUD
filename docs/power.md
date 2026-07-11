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
2. Splash full-refresh (**inverted**: black bg, white text + bike silhouette)  
3. **Release** the button completely  
4. Device stays on splash (MCU idle, BLE off / paused)  
5. Hold power ≥ **500 ms** again to wake → BLE + UI  

Deep sleep was bouncing (black → white → ride UI) before the splash could stick, especially with USB-Serial/JTAG. Soft-sleep is intentional until battery deep-sleep is revalidated.

### Wake / auto-sleep pitfalls (fixed)

| Bug | Failure | Fix |
|---|---|---|
| NimBLE `deinit` while connected | heap free assert, bounce off splash | `ble_service_shutdown_for_sleep()` pauses only |
| Stale `buttons_power_held_ms` | soft-wake immediately re-sleeps | `buttons_power_reset_hold()` + ~800 ms sleep guard |
| Mid-loop `power_note_activity()` then `poll(stale_now)` | unsigned `(now - activity)` underflows → “10 min idle” after a few seconds | re-read `millis()` + signed idle delta |

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
[ble] shutdown for sleep
[ble] radio paused (no deinit)
[power] drawing splash
[power] splash done
[power] wait for release
[power] armed — hold power to wake
… (hold to wake) …
[power] wake press accepted
[power] restoring display + BLE
[power] === AWAKE ===
```

If you never see `SLEEP BEGIN`, the hold detector isn’t firing.  
If you see `AWAKE` then immediately another `SLEEP BEGIN`, the post-wake hold/guard path is broken.  
If you never leave the splash after arming, the wake long-press isn’t accepted.

## If wake fails

1. Hold power 1–2 s  
2. Plug USB-C  
3. `pio run -e x4 -t upload`  
