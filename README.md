<!--
SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
Copyright (c) 2026 AgingGliders
-->

# AgingGliders Recorder Firmware

Firmware for the AgingGliders recorder prototype based on the Waveshare ESP32-S3 AMOLED 2.06 board.

The recorder samples acceleration at 20 Hz, stores recordings on SD card, provides a local LVGL user interface, and exposes a WiFi/Web interface for file management and calibration.

## 1. Hardware

Target device:

```text
Waveshare ESP32-S3 AMOLED 2.06
```

The board is programmed through the Arduino IDE using the ESP32-S3 board support package.

## 2. Arduino IDE Board Configuration

In the Arduino IDE, select:

```text
Board:             ESP32S3 Dev Module
Port:              COM port connected to the recorder board
Partition Scheme:  Huge APP
PSRAM:             OPI PSRAM
```

The exact COM port depends on your PC and connected board.

## 3. Required Libraries

Install the following Arduino libraries and keep them at the listed versions.

Later versions may not be compatible with this firmware without code changes.

| Library | Author / Publisher | Version |
|---|---|---:|
| Async TCP | ESP32Async | 3.4.10 |
| ESPAsync WebServer | ESP32Async | 3.11.0 |
| GFX Library for Arduino | Moon On Our Nation | 1.6.0 |
| SensorLib | Lewis He | 0.3.1 |
| XPowersLib / XPowerLib | Lewis He | 0.3.3 |
| LVGL | Kisvegabor / LVGL project | 9.3.0 |

## 4. Project-local configuration files

The project includes configuration headers that must stay with the source tree:

```text
lv_conf.h
src/board/pin_config.h
src/ui/ui_definitions.h
```

`lv_conf.h` is the LVGL configuration file for LVGL 9.3.0. It is intentionally stored at the project root so LVGL can find it without requiring a copy in the global Arduino libraries folder.

`src/board/pin_config.h` contains the Waveshare ESP32-S3 AMOLED 2.06 board pin mapping used by the firmware.

`src/ui/ui_definitions.h` contains UI-only layout, font-alias, button-size, and LVGL draw-buffer definitions. Operational recorder constants remain in `config.h` or `src/global.h`.

Do not rely on copies of these files in `Documents/Arduino/libraries`.

## 5. Installation Steps

1. Install the Arduino IDE.
2. Install the ESP32 board package.
3. Install the required libraries listed above at the specified versions.
4. Open the recorder firmware project in the Arduino IDE.
5. Select the board and board options listed in Section 2.
6. Connect the recorder board by USB.
7. Select the matching COM port.
8. Compile the firmware.
9. Upload the firmware to the board.

After uploading firmware from the Arduino IDE, perform a full device restart before formal testing. This avoids occasional transient startup issues after the IDE upload/reset sequence.

## 6. First Run / Setup

On first boot, the recorder may show setup or calibration messages.

Typical required setup:

1. Configure date.
2. Configure time.
3. Configure glider registration.
4. Configure WiFi password.
5. Start WiFi and perform accelerometer calibration from the Web interface.

The recorder will not authorize recording until required settings are stored and a valid calibration exists.

## 7. WiFi / Web Interface

When the recorder is READY, open MENU and select START WIFI.

The recorder starts an access point and Web server at:

```text
http://192.168.4.1/
```

The Web interface supports:

- SD file listing;
- file download;
- file archive;
- calibration.

The Web delete action archives files by moving root recording files to:

```text
/processed
```

It does not erase the file contents from the SD card.

## 8. SD Card Behavior

Recording requires:

- SD card present;
- sufficient free space;
- root recording file count below the configured limit;
- valid settings;
- valid calibration.

Important SD conditions:

```text
SD FULL (FILES)
```

This means the SD root file-count limit has been reached. It can be resolved from the recorder by using:

```text
MENU -> START WIFI -> Web file archive
```

Archiving moves root files to `/processed`, reducing the root file count.

```text
SD LOW
```

This means SD free space is below the configured threshold. Archiving does not solve this, because moving files to `/processed` does not free SD memory.

## 9. Artificial Intelligence Assistance

This project was developed with significant assistance from artificial intelligence tools.

AI contributed substantially to the architecture and coding of some software areas, including the graphical user interface and Web services. For core recorder functions, including the state machines, helper layers, abstraction layers, and overall system architecture, the design decisions and implementation remained under developer control. In those areas, AI was still used to review, clean, reorganize, homogenize naming and comments, identify issues, and propose corrections.

Overall, AI was instrumental in producing an operational demonstrator in approximately two weeks and in helping mature the firmware into a first-release candidate in less than four months.

## 10. Licensing

Project-owned AgingGliders recorder firmware code is licensed for non-commercial use under:

```text
PolyForm Noncommercial License 1.0.0
```

Commercial use requires prior written permission from AgingGliders.

See:

```text
LICENSE
COMMERCIAL-LICENSE.md
THIRD_PARTY_NOTICES.md
THIRD_PARTY_VERSIONS.md
```

Third-party libraries remain under their own licenses and are not relicensed by AgingGliders.

## 11. Commercial Licensing Contact

For commercial licensing, contact:

```text
aginggliders@gmail.com
```
