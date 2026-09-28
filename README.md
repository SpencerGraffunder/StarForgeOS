# StarForgeOS — NuclearCounter Dual-Boot Fork

This is a fork of [RaceFPV/StarForgeOS](https://github.com/RaceFPV/StarForgeOS), the StarForge ESP32 drone race timing system.

## Purpose

Enable **dual boot** of StarForgeOS with the **NuclearCounter** firmware (a 5.8 GHz scanner for FPV drones) on the NuclearCounter board:

- Board support for the **ESP32-S3** (NuclearCounter V3.0) and **ESP32-C3** (NuclearCounter V2.1)
- StarForgeOS lives in the `ota_1` slot, the NuclearCounter firmware in `ota_0`; both share the same buttons, OLED, buzzer, and battery
- Switch boot from either firmware's menu: **"Boot Scanner Mode"** in StarForgeOS, the ⭐ item in the NuclearCounter menu

## Changes from upstream

- `platformio.ini`: `nuclearcounter` (C3) and `nuclearcounter_s3` (S3) envs — board pins, 4 MB board file, dual-boot partition tables
- Dual-boot partition tables (`partitions_dualboot.csv`, `partitions_dualboot_c3.csv`) and a 4 MB S3 board file (`boards/esp32-s3-devkitc-1-4MB.json`)
- `src/hardware/board_displays.{h,cpp}` — NuclearCounter OLED menu: mode switching (standalone / USB node / dual-boot to scanner), battery voltage readout, navigation beeps, low-battery alarm; `bootScannerMode()`, `bootNodeMode()`, `bootStandalone()`
- `src/hardware/nc_buzzer.{h,cpp}`, `src/hardware/nc_battery.{h,cpp}` — buzzer + battery modules matching the NuclearCounter firmware's pins and behavior
- `src/settings/nc_settings.{h,cpp}` — reads the buzzer on/off and battery alarm threshold from the NuclearCounter firmware's NVS settings store, so both firmwares on the board share one configuration
- `src/settings/config_loader.{h,cpp}` — NVS flag for the node/standalone boot mode
- Standalone web UI (`data/`): RSSI graph, lap list, stats, and a notification bar
- WiFi AP on `192.168.8.1` and `esp_wifi_set_max_tx_power(20)` (required for the S3 beacon on this hardware)
- Boot-menu item renamed to **"Boot Scanner Mode"** (the other firmware on this board is the NuclearCounter scanner)

Everything else is upstream StarForgeOS. Build it through the
[NuclearCounter repo's CI](https://github.com/SpencerGraffunder/NuclearCounter)
(see that repo's README) or locally with PlatformIO.
