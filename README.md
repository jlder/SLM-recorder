<!--
SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
Copyright (c) 2026 AgingGliders
-->

# AgingGliders Recorder Firmware

Firmware for the AgingGliders recorder prototype based on the Waveshare ESP32-S3 AMOLED 2.06 board.

The recorder samples acceleration at 20 Hz, stores recordings on SD card, provides a local LVGL user interface, and exposes a WiFi/Web interface for file management, calibration, and firmware update.

## 1. Hardware

Target device:

```text
Waveshare ESP32-S3 AMOLED 2.06
```

The board is programmed through the Arduino IDE using the ESP32-S3 board support package.

## 2. Arduino IDE Board Configuration

After Arduino installation, from the Arduino IDE menu File / Preferences, add the Espressif board manager URL:

https://espressif.github.io/arduino-esp32/package_esp32_index.json

In the Arduino IDE Board Manager, install `esp32` from Espressif at version 3.3.5.
Keep the ESP32 board package at version 3.3.5. Later ESP32 board package versions may not be compatible with the listed Arduino_GFX version.

Then in the Arduino IDE menu  Tools, select:

```text
Board:             ESP32S3 Dev Module
Port:              COM port connected to the recorder board
USB CDC:           Enabled
Flash Size:        16 MB (128 Mb)
Partition Scheme:  Custom
PSRAM:             OPI PSRAM
```

The exact COM port depends on your PC and connected board.
The custom partition scheme is described in the project-root `partitions.csv` file. It provides two 3.5 MB OTA application slots and the remaining flash space as FATFS.

## 3. Required Libraries

Install the following Arduino libraries and keep them at the listed versions.

Later versions may not be compatible with this firmware without code changes.

The FT3168 touch controller is handled directly by the project firmware and does not require Arduino_DriveBus.

| Library | Author / Publisher | Version |
|---|---|---:|
| Async TCP | ESP32Async | 3.4.10 |
| ESPAsync WebServer | ESP32Async | 3.11.0 |
| GFX Library for Arduino | Moon On Our Nation | 1.6.0 |
| SensorLib | Lewis He | 0.3.1 |
| XPowersLib | Lewis He | 0.3.0 |
| lvgl | Kisvegabor / lvgl project | 9.3.0 |

## 4. Project-local configuration files

The project includes two configuration headers that must stay with the source tree:

```text
lv_conf.h
src/board/pin_config.h
```

`lv_conf.h` is the LVGL configuration file for LVGL 9.3.0. It is intentionally stored at the project root so lvgl can find it without requiring a copy in the global Arduino libraries folder.

`src/board/pin_config.h` contains the Waveshare ESP32-S3 AMOLED 2.06 board pin mapping used by the firmware.

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
9. Upload the firmware to the board with USB.
10. After the first USB firmware upload and initial configuration (see Section 6), later firmware updates may be installed from the Web interface using Firmware Update.

After uploading firmware from the Arduino IDE, perform a full device restart before formal testing. This avoids occasional transient startup issues after the IDE upload/reset sequence.

Alternative firmware installation methods:

- For first application installation, it is possible to install the binary file located in the `docs/firmware` folder (path defined in `manifest.json`) using the GitHub-hosted WebUSB installer: https://jlder.github.io/SLM-recorder/. When connected, the device should be visible under the USB JTAG/serial debug interface.
- When the application has been installed once, subsequent updates can be performed using OTA: start device WiFi, open `http://192.168.4.1/`, select Firmware Update, and upload an application binary named like `SLM_recorder_date_version.bin` from the project-root `firmware` folder.

/!\ In both cases it is recommended to use Chrome in incognito mode to avoid using cache with outdated information.

## 6. First Run / Setup

On first boot, the recorder may show setup or calibration messages.

Typical required setup:

1. Configure date.
2. Configure time.
3. Configure glider registration.
4. Configure WiFi password.
5. Perform calibrations using the Web interface (see section 8).

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
- browser-side flight-time analysis during download, displaying detected flight times and sample-period average/standard deviation, with HIRMS-gated takeoff/landing validation and transition debounce;
- file archive through the root file-list Archive button, which moves the file to `/processed`;
- permanent deletion of selected files already archived in `/processed` from the Maintenance / Delete page;
- calibrations;
- firmware update using an application binary named like `SLM_recorder_date_version.bin` (firmware update is accepted only when USB power is connected to the device);
- a lightweight health-check endpoint at `http://192.168.4.1/diag`.

For this release baseline, the browser-side analysis is limited to flight-time detection and sample-period statistics; Kossira occurrence/load-factor processing and CSV export are not active.

When WiFi/Web access is active, the screen START RECORD button is disabled/grey. Stop WiFi before starting recording from the screen. The physical RECORD button remains available as an independent hardware control; if it starts recording while WiFi is active, the recorder leaves READY and stops WiFi/Web before entering recording.

Implementation note: the HTTP listener is created, route-registered, and started once. START WIFI / BACK only start and stop the ESP32 access point and Web-side application activity. The firmware intentionally does not call `AsyncWebServer::end()` during normal Web OFF because the selected AsyncWebServer/AsyncTCP stack does not provide a reliable stop/restart lifecycle for port-80 dispatch after HTTP traffic.

## 8. Calibrations

There is a recorder accelerometer calibration which samples accelerations on the sensor six faces to adjust gains/offsets on the 3 axes. The recorder must be placed successively on its six faces while waiting for each face to show a stable capture. There is also an installation calibration which compensates for possible pitch/roll mounting-angle errors. Its goal is for the recorder to report a Z axis aligned with gravity when the glider is in flight attitude with wings leveled. The recorder shall be in its support in the glider when performing this calibration. During installation calibration, the Web page follows the current stable attitude; Save is enabled only while the current rolling window is stable.

Note: the installation calibration corrects pitch/roll mounting error. Yaw around the vertical axis is not observable from gravity alone and is not corrected. Recording remains disabled until both the sensor calibration and the installation calibration are valid.

## 9. SD Card Behavior

Recording requires:

- SD card present;
- free space above the configured recording-start threshold;
- root recording file count below the configured limit;
- valid settings;
- valid calibrations.

Recording files use a daily-file policy. The first recording session of a day creates a file named:

```text
/REGISTRATION_YYYYMMDD_1.bin
```

For each subsequent session on the same day, the existing daily file is renamed to the next session count, for example `_2.bin`, and the session data is appended to that same file. The suffix is the number of recording sessions contained in the daily file.

Important SD conditions:

```text
SD FULL (FILES)
```

This means the SD root file-count limit has been reached. It can be resolved by using the Web interface to upload files to the central SLM server and archive root files to `/processed`, then by deleting archived files from `/processed` if space must be freed.

```text
SD LOW
```

This means SD free space is below the configured low-space threshold. Archiving does not solve this, because moving files to `/processed` does not free SD memory. The Maintenance/Delete function can remove files from `/processed`, but deleted files are permanently lost.

The firmware uses two SD free-space thresholds: a higher threshold required before recording starts and a lower threshold while recording is already active. This prevents recording from starting just above the low-space threshold and immediately stopping with `SD LOW`.

## 10. Artificial Intelligence Assistance

This project was developed with significant assistance from artificial intelligence tools.

AI contributed substantially to the architecture and coding of some software areas, particularly the graphical user interface and Web services. For core recorder functions, including the state machines, helper layers, abstraction layers, and overall system architecture, the design decisions and implementation remained under developer control. In those areas, AI was still used to review, clean, reorganize, homogenize naming and comments, identify issues, and propose corrections.

Overall, AI was instrumental in producing an operational demonstrator in approximately two weeks and in helping mature the firmware into a first-release candidate in less than four months.

## 11. Licensing

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

## 12. Commercial Licensing Contact

For commercial licensing, contact:

```text
aginggliders@gmail.com
```
