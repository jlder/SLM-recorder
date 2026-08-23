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
4. Perform calibrations using the Web interface (see section 8).

The recorder will not authorize recording until required settings are stored and a valid calibration exists.

## 7. WiFi / Web Interface

When the recorder is READY, open MENU and select START WIFI.

The WiFi SSID is `SLM2-` followed by the stored registration. The `2` identifies WiFi connection generation 2 and changes only when an incompatible SSID/password or connection solution is introduced. The WiFi password is generated automatically as `SLM` followed by the stored five-character registration in reverse order. Example: registration `FCJAF` gives password `SLMFAJCF`.

The recorder starts an access point and Web server at:

```text
http://192.168.4.1/
```

The Web interface supports:

- SD file listing;
- file download;
- browser-side flight-time analysis during download, displaying detected flight times and sample-period average/standard deviation, with HIRMS-gated takeoff/landing validation and transition debounce;
- file archive through the root file-list Archive button, which moves the file to `/processed`;
- a Logbook view showing the five newest archived `.log` files, presented as the last five flying days;
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

Recording files use an immutable session-file policy. The first recording session of a day creates:

```text
/REGISTRATION_YYYYMMDD_1.bin
```

Each subsequent same-day session creates a new file with the next suffix, for example `_2.bin` and `_3.bin`. The recorder scans both the SD root and `/processed` when allocating the suffix, so an archived session number is never reused. Closed recording files are never renamed, restored, reopened, or appended. Legacy appended files remain readable and are left unchanged.

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

## Firmware from server through SLM Bridge

The Firmware Update page can use SLM Bridge to install recorder firmware from the server while keeping the operator workflow in the recorder Web UI. The bridge searches the connected recorder's server folder `<registration>/FIRMWARE` first. If that folder does not exist or contains no accepted recorder firmware `.bin`, it falls back to the common `SLM-STC-DATA/FIRMWARE` folder. The operator selects the firmware file to upload; the feature is intentionally named "Firmware from Server" rather than "latest firmware" because older versions may be selected for recovery or test work.

### Recorder file processing lock (v1.26)

The root file-list action is labelled **Process** when the Android bridge automatic workflow is available. The selected file is locked immediately and its button is grey and inactive while it is downloading, being analysed, queued, uploading, or being archived. The row disappears after successful archive. A definite download or analysis failure clears the lock and restores the blue **Process** button. The Android bridge remains the authoritative source of pending file states, so a page refresh or reconnection does not re-enable a file that is still in the durable queue.

### Version 1.29

- Corrected ES8311 alert timing by pacing I2S PCM writes in real time.
- Error alert is now three equal 250 ms beeps separated by 250 ms silence.
- Added 100 ms trailing silence before muting the codec/amplifier.

### Version 1.28

- File processing is serialized through the local Downloading and Analyzing phases. While either phase is active, every other Process button is grey and inactive. Other files become processable again as soon as the active file reaches Queued or if processing fails.

## Audible error alert (v1.28)

The recorder uses the Waveshare ESP32-S3-Touch-AMOLED-2.06 ES8311 codec and
on-board speaker to attract attention while the recorder is in `ST_ERROR`.
The alert consists of three equal 250 ms beeps separated by 250 ms silence and
repeated every four seconds. A 300 ms silent hardware warm-up occurs before the
first beep so the three audible tones sound identical. Pressing PWR/CLR
acknowledges and silences the current audible alert even when the
underlying error condition is still present. A different error, or a new entry
into `ST_ERROR` after the error has cleared, re-arms the alert.

The audio feature is auxiliary and fail-silent. Its low-priority task remains
dormant outside `ST_ERROR`; the codec output and speaker amplifier are disabled
outside an active, unacknowledged alert. Audio initialization or playback
failure does not raise a recorder error and does not affect acquisition,
recording, SD handling, or state-machine timing.


### Version 1.31

- Recording files are now immutable per session. Each recording start creates a new `REGISTRATION_YYYYMMDD_N.bin` file.
- The next suffix is selected from matching `.bin` files in both the SD root and `/processed`.
- Closed files are never renamed, restored, reopened, or appended. Legacy appended files remain supported and unchanged.


### Version 1.30

- Added a 300 ms silent audio pre-roll after enabling the ES8311 playback path and speaker amplifier.
- The pre-roll primes the codec and I2S DMA before the first audible tone so all three error beeps have the same duration and sound.


## v1.32 immutable-file SHA

Each new immutable recording is hashed while its bytes are written. On close, the recorder writes a companion `.sha` metadata file containing format version, filename, byte length, and SHA-256. Legacy `.bin` files without metadata remain supported. The authenticated About page provides **Verify Recordings**, which rereads pending root files with `.sha` metadata and reports valid, legacy, metadata-error, and mismatch counts.

## v1.33 daily File Management view

File Management presents one logical entry per registration/date while immutable recording files remain separate on SD and on the server. Pressing **Process** sequentially downloads, verifies, analyses, queues, uploads, verifies, and archives every pending suffixed `.bin` file for that day. The visible progress is calculated from aggregate pending `.bin` bytes; physical suffixes and file counts are not exposed to the user.

## Current development baseline (v1.55 field-validation candidate)

Recorder v1.55 is a dedicated automation field-validation build based on the v1.53 State-ownership/automation architecture and remains intended to interoperate with the existing SLM Bridge protocol. It is deliberately **observe-only**: automation detectors and virtual policy run continuously, but AUTO RECORDING, AUTO WIFI, and AUTO DELETE are not allowed to actuate recorder state, WiFi, or file deletion. Manual controls, faults, and the <=5% battery shutdown remain real.

For this field-validation build:

- AUTO RECORDING, AUTO WIFI, and AUTO DELETE are forced logically **ON from boot**. SETTINGS > AUTOMATION shows all three as ON with disabled controls. Stored NVS selections are not rewritten and are ignored while the diagnostic override is compiled.
- The intended field procedure is one manually started morning-to-evening recording. Virtual AUTO sessions run inside that physical file so multiple flights and nuisance cycles can be evaluated without automation changing the recorder state.
- AUTO START is confirmed by either the existing 2 s three-axis motion RMS >=0.020 g for 1 s or a first-order 0.10 Hz high-pass on installation-aligned Nx/Ny with `max(|HP(Nx)|, |HP(Ny)|) >= 0.020 g` for 1 s.
- Before `flight_seen`, the virtual AUTO session retains the 300 s continuous quiet nuisance timeout. Once `flight_seen` latches, motion quiet cannot end the virtual flight session.
- Flight end reuses the continuous causal HIRMS/LOWRMS signals. A valid landing event requires HIRMS >=0.050 g for >=3 s, with normalized FlightGround >=0.10 during that same HIRMS event; after HIRMS falls below 0.050 g, FlightGround <=0.020 for 2 s within 25 s creates GROUND. FlightGround >=0.10 cancels GROUND; 50 s continuous GROUND confirms the virtual flight end.
- AUTO DELETE is evaluated virtually only. A virtual automatic no-flight session logs `WOULD_AUTO_DELETE`; the physical `.bin` and `.sha` are never deleted by automation in v1.55.
- AUTO WIFI is evaluated virtually only. The trace logs the intended policy: ON in virtual READY, OFF for confirmed motion or virtual recording, and ON again after 5 s quiet. Actual manual WiFi request/AP transitions are logged separately.
- The diagnostic event stream is embedded reversibly in the SD-bound Nx/Ny/Nz copy only. The current sample is pushed first; diagnostic transitions are evaluated afterward and therefore normally appear one 50 ms sample later. `tools/automation_diag_decode.py` restores the original acceleration stream exactly and writes a separate event CSV.
- **v1.55 file-analysis correction:** the recorder Web SLM decoder now removes the same reversible +/-20.001 g diagnostic axis offsets immediately after validating each stored 0x70 checksum and before converting acceleration to g or running HIRMS/LOWRMS flight analysis. This keeps diagnostic files analyzable directly on the recorder while leaving the stored diagnostic trace unchanged.
- With USB absent, the display enters full standby after the normal inactivity delay. With USB present, the display remains on and dims to about 50% (`128/255`) after the same delay. Local activity restores full brightness.
- Battery <=5% remains an unconditional controlled stop-and-shutdown threshold.

Historical replay of the frozen AUTO logic on the current official/recovered data gives 190/190 pre-roll AUTO START coverage and 189/189 evaluable flight-end confirmations with zero premature stops and zero misses; `FCJAF_20260627_3.bin` is right-censored because its historical file ends at landing and contains no post-landing confirmation interval.

The inherited v1.52 baseline also includes:

- **French/English recorder interface:** French is the default recorder language. SETTINGS provides an ENGLISH/FRANÇAIS toggle stored in NVS. The selection controls both the AMOLED interface and recorder-served Web pages through the central `src/services/language.h` catalog; protocol/API reason codes and persistent calibration-report formats remain language-independent.
- **Self-contained recorder Web page:** the browser translation catalog is generated from `language.h` and embedded directly in the served page, avoiding the additional `/api/language.js` request introduced in v1.46. Since v1.50 the page is assembled per request from PROGMEM segments so that only the recorder-selected language is transmitted, the language code is already correct in the first bytes the browser parses, and the page carries a version-and-language `ETag` so a returning browser revalidates with `304` instead of downloading the whole document again.
- **Installation calibration restore after recorder replacement:** Maintenance > About can restore the newest valid installation-calibration report matching the configured registration from `/calibration_reports` or `/processed` into the installation-calibration NVS record. Recorder six-face calibration remains independently required on the replacement hardware.
- **USB-power Web gating:** File Management, Logbook, firmware update, installation calibration, report handling, and support actions require USB power. The Main Menu explains the restriction when USB is absent; Recorder Calibration remains available through Maintenance on battery power.
- **Daily processing status:** Flight Analysis shows real `Processing: N%` progress across all physical files for the selected day. Recorder transfer contributes 95% and browser analysis 5% of each file's size-weighted contribution. Final flight details are shown only after analysis results are available; failures give an actionable retry message.
- **Simplified Firmware Update page:** firmware is selected in one card from the server (preferred) or from the phone when requested by support, followed by a common Update Firmware / Return card. The page warns that Wi-Fi is lost during the successful restart and must be restarted before reconnecting.

The v1.52 baseline retains the normal French accents in the central translation catalog. The AMOLED keeps LVGL's built-in Montserrat fonts for ASCII and uses compact accent-capable wrapper fonts that synthesize only the nine French glyphs required by the current catalog (`À Ç É Ê à ç è é ê`). The recorder Web page uses the same UTF-8 translations directly.

The v1.52 baseline increases the successful OTA acknowledgement-to-reboot delay from 500 ms to 2000 ms so the asynchronous HTTP/TCP stack has more time to deliver the final `200 / ok` response before Wi-Fi disappears during restart. The support-reboot delay remains 500 ms.

### Web interface responsiveness (v1.50)

Before v1.50 the recorder page emitted every label as an empty element carrying a `data-i18n` key and filled them from JavaScript on the last line of the document. The three main menu buttons therefore appeared as plain coloured rectangles until the whole page had been transferred and parsed. v1.50 applies the static translations immediately after the body markup instead, sends one language rather than both, and injects the selected language code server-side, which also removes the full-page re-translation that previously followed the first `/api/status` response. The served document is about 183 kB instead of 194 kB, and the menu labels are present roughly 44 kB into the stream instead of at the final byte.

### Download read-ahead (v1.51)

Every `sd_files` operation is a synchronous handshake with `sd_task`, and the HTTP response filler is called once per TCP window. Reading directly per chunk therefore cost one handshake per 1436 bytes. v1.51 reads one 32 KiB block into a PSRAM buffer and serves the transmitted chunks from it, cutting the handshakes for a one-megabyte file from roughly 730 to 32 and raising measured download throughput from about 0.5 MB/s to about 0.7-0.8 MB/s. If PSRAM cannot be allocated the original direct per-chunk path is used unchanged.

Measurements taken with a diagnostic endpoint that streamed a RAM pattern with no SD access showed that the remaining transfer time is dominated by the WiFi link rather than by SD access. Throughput is limited by the fixed 5744-byte lwIP send buffer divided by the client round-trip time, which is why it varies from about 0.6 MB/s to about 1.0 MB/s depending on the client. The diagnostic endpoint was removed once the measurements were complete.

### Calibration driver publication (v1.51)

`calibration_service_refresh_status()` previously published the accelerometer calibration to `accel_driver` on every path, so `state_task` rewrote the driver coefficients twenty times a second and Web status polling wrote them as well. v1.51 separates the two concerns. Status recalculation stays where it was; publication moves to `calibration_service_publish_driver_state()`, which is called only by `state_task` and writes to the driver only when the effective calibration has actually changed. In steady state no driver write occurs. `calibration_service_refresh_status()` now takes the calibration-service mutex, because Web operator calibration actions call it from the asynchronous HTTP task.

The repository firmware manifest remains on the last compiled/hosted field image, v1.54 (`SLM_recorder_20260823_v1_54.merged.bin`). It shall be advanced to v1.55 only after a real ESP32-S3 v1.55 merged binary is compiled and hosted.


### Runtime settings ownership

The State task is the sole runtime authority for recorder setting changes and WiFi enable decisions. UI callbacks post one-shot requests; they do not call Preferences-backed setters or `web_task_set_enabled()` directly. `settings_init()` performs only boot-time NVS loading/migration before tasks start.
