# Flash BikeHUD & restore CrossPoint

## Short answer

| Risk | Reality |
|---|---|
| Permanent hardware brick? | **No** — ESP32-C3 always has a ROM USB download mode. A bad app is reflashable. |
| Temporarily “bricked” as a reader? | **Yes, by design** — BikeHUD replaces CrossPoint in SPI flash until you flash CrossPoint again. |
| Lose books / reading progress? | **No, if they live on the SD card** — CrossPoint caches library state under `/.crosspoint/` on the microSD. BikeHUD does not touch the SD. |
| Lose CrossPoint Wi‑Fi / NVS prefs? | **Maybe** — those live in on-chip flash (NVS). A full 16 MB backup restores them; a bare app reflash may not. |

**Do not flash BikeHUD on a USB-locked AliExpress unit** unless you understand CrossPoint’s unlocker warning: after re-lock, recovery needs USB or OTA. BikeHUD has **no OTA**, so a locked device with no USB path can get stuck. Units from **xteink.com** are not USB-locked.

---

## What lives where

```
┌─ microSD (untouched by BikeHUD) ─────────────────────────┐
│  Books (.epub, …)                                         │
│  /.crosspoint/   settings, progress, caches, recent      │
│  /fonts/ …                                                │
└──────────────────────────────────────────────────────────┘

┌─ Internal 16 MB SPI flash (overwritten by flash tools) ──┐
│  Bootloader + partition table                             │
│  app0 / app1  ← CrossPoint  XOR  BikeHUD                  │
│  NVS (Wi‑Fi creds, small prefs)                           │
│  SPIFFS (rarely critical for CrossPoint day-to-day)       │
└──────────────────────────────────────────────────────────┘
```

CrossPoint stores almost all reader state on SD ([their README](https://github.com/crosspoint-reader/crosspoint-reader)):

```text
.crosspoint/
├── epub_<hash>/     # progress, covers, chapter layout cache
├── settings.json
├── state.json
└── recent.json
```

BikeHUD currently **does not mount or write the SD card**. Leave the card in (or pull it — either is fine).

Our `partitions.csv` matches the common X4 / CrossPoint 16 MB layout so CrossPoint app images remain flashable at `0x10000`.

---

## Before first custom flash — full backup (do this once)

Wake the X4, plug USB‑C (data cable), then:

```bash
# Find port (macOS examples: /dev/cu.usbmodem* /dev/cu.wchusbserial*)
ls /dev/cu.usb* /dev/cu.wch* 2>/dev/null

# Full 16 MB dump — gold standard restore of *exactly* what’s running now
esptool.py --chip esp32c3 --port /dev/cu.usbmodemXXXX \
  read_flash 0x0 0x1000000 \
  backups/x4-full-$(date +%Y%m%d).bin
```

Store that file somewhere safe (not only on this machine’s `/tmp`).  
Time: a few minutes at default baud; add `--baud 921600` if stable.

Optional: also save a known-good **CrossPoint release** `firmware.bin` from  
https://github.com/crosspoint-reader/crosspoint-reader/releases

---

## Flash BikeHUD

```bash
cd firmware
pio run -e x4          # or x4_demo for panel-only
pio run -e x4 -t upload
pio device monitor     # expect "=== BikeHUD X4 ==="
```

PlatformIO writes bootloader + partitions + app (not the microSD).  
After flash, the device boots BikeHUD, not CrossPoint, until you restore.

---

## Restore CrossPoint (pick one)

### A. Web flasher (easiest)

1. Wake X4, USB‑C to computer, **Chrome**.
2. https://crosspointreader.com/#flash-tools  
3. Select **X4** → latest CrossPoint (or **Custom .bin** with a release `firmware.bin`).
4. Flash, unplug/replug or reset if needed.

Same site can flash **stock Xteink** firmware if you want official, not CrossPoint.

### B. esptool — CrossPoint app image (usual)

Matches CrossPoint’s documented CLI:

```bash
esptool.py --chip esp32c3 --port /dev/cu.usbmodemXXXX --baud 921600 \
  write_flash 0x10000 /path/to/crosspoint-firmware.bin
```

Use a release **`firmware.bin`** (app), not a random partial dump.

### C. esptool — full backup restore (exact prior state)

If you took the 16 MB dump:

```bash
esptool.py --chip esp32c3 --port /dev/cu.usbmodemXXXX --baud 921600 \
  write_flash 0x0 backups/x4-full-YYYYMMDD.bin
```

This restores bootloader, partitions, apps, NVS, SPIFFS — whatever was on the chip at backup time.

### D. Stock / factory without your own dump

- Web flasher official image, or  
- Community full dumps exist (e.g. Adafruit’s X4 backup guide) — prefer **your** dump when possible.

---

## After restoring CrossPoint

1. SD card still has books + `/.crosspoint/` → library and progress should reappear.  
2. If Wi‑Fi is gone, re-enter it (NVS not restored unless you used a full dump).  
3. If something looks stale after weird experiments, delete `/.crosspoint` on the SD (forces cache rebuild; **reading progress in those caches is wiped** — only do this if needed).

---

## If the screen is blank / no serial

1. Hold power / reset; try again.  
2. Confirm data-capable USB cable and port: `ls /dev/cu.usb*`.  
3. Force download mode if needed (reset while holding BOOT if your board exposes it; many X4s enter download on normal `esptool` connect).  
4. Reflash CrossPoint via web tool or full backup.  
5. Still no USB device at all → USB lock or cable/power issue, not “BikeHUD burned the chip.”

---

## Policy for this project

- **Always** keep a full 16 MB backup before the first BikeHUD flash.  
- Prefer unlocking only official paths; never assume BikeHUD can OTA itself back to CrossPoint.  
- Treat BikeHUD as a **swap firmware** for rides, not a permanent replacement unless you want that.
