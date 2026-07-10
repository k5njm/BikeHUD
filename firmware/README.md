# X4 firmware (BikeHUD)

PlatformIO firmware for the XTEink X4: NimBLE **peripheral** + e-ink HUD.

## Build

```bash
cd firmware
pio run                 # release stack with BLE
pio run -e x4_demo      # synthetic numbers, no BLE (desk panel test)
pio run -t upload
pio device monitor
```

Include path pulls `../protocol/bike_hud_protocol.h`.

## Verify BLE without an app

1. Flash default `x4` env.
2. Open [nRF Connect](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile) on a phone.
3. Connect to **BikeHUD**.
4. Find service `B10E0001-C0C0-41A3-B4C6-42494B454855`.
5. Write 16 bytes (hex) to characteristic `…0002…`:

```
01 15 BC 02 68 30 4C 0F 94 FF 38 01 49 05 01 00
```

Digits should jump; serial should log the packet and free heap.

## Heap target

With BLE connected and 1 Hz writes, aim for **> 80 KB** free. Log line every 5 s:

```
[mem] free=… min=… connected=1 pkts=…
```

## Backup & restore CrossPoint

See **[`docs/flash-and-restore.md`](../docs/flash-and-restore.md)**.

```bash
# Once, before first flash
../scripts/backup-x4.sh /dev/cu.usbmodemXXXX

# Later: CrossPoint release app image, or full dump
../scripts/restore-crosspoint.sh ~/Downloads/crosspoint-firmware.bin
../scripts/restore-crosspoint.sh --full ../backups/x4-full-….bin
```

BikeHUD overwrites SPI flash only (app/bootloader). The microSD (books, `/.crosspoint/`) is left alone.

## Notes

- SPI pins are non-default — see `include/board_pins.h`.
- Partial refresh + full clear every ~50 s in `hud.cpp`.
- Rotation may need tweak for your stem mount (`display.setRotation`).
