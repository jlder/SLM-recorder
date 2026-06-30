<!--
SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
Copyright (c) 2026 AgingGliders
-->

# Recorder Requirements

## 1. Purpose

This document captures recorder requirements in three parts:

1. operational requirements, stated from the user/device point of view;
2. implementation allocation, mapping requirements to code modules;
3. validation and evidence notes.

Architecture rationale is in `02_recorder_architecture.md`.
State-machine behavior is in `03_state_machine_behavior_review.md`.
Validation strategy is in `05_lightweight_validation_strategy.md`.

## 2. Requirement Types

| Prefix | Meaning |
|---|---|
| `OP-*` | Operational requirement |
| `CFG-*` | Configured limit or timing parameter |
| `VAL-*` | Validation case from `05_lightweight_validation_strategy.md` |


### OP-BUILD-001 — Project-local configuration headers

The firmware source release shall include the board pin mapping and LVGL configuration headers required for compilation.

Required project-local files:

```text
src/board/pin_config.h
lv_conf.h
partitions.csv
```

The firmware shall not require these files to be manually copied into the global Arduino libraries folder.

`partitions.csv` shall define the project-local flash partition layout used for OTA-capable builds.

Status:

- **Implemented.**

## 3. Configuration Values

The following values are currently compiled in software and can be changed in `config.h` or `src/global.h`.

The table lists values that affect operational behavior, operator-visible behavior, recording format, timing, validation, or the rationale for task/core allocation. Low-level sizing values such as task stack sizes are not listed unless they directly support an operational requirement.

### 3.1 Operator controls and button timing

| Configuration | Current value | Requirement use |
|---|---:|---|
| `GPIO_RECORD_SWITCH` | 0 | physical record/start-stop button input |
| `GPIO_POWER_BUTTON` | 10 | physical power/clear button input |
| `POWER_CLEAR_HOLD_MS` | 150 ms | error clear / acknowledgement |
| `POWER_SHUTDOWN_HOLD_MS` | 2000 ms | application-level shutdown request |
| `RECORD_START_HOLD_MS` | 500 ms | recording start gesture |
| `RECORD_STOP_HOLD_MS` | 2000 ms | recording stop gesture |

The current configured shutdown hold time of 2000 ms and record-start hold time of 500 ms are accepted operational values.

### 3.2 Display and UI

| Configuration | Current value | Requirement use |
|---|---:|---|
| `DISPLAY_BRIGHTNESS_ACTIVE` | 255 | active display brightness |
| `DISPLAY_BRIGHTNESS_DIMMED` | 60 | legacy dimmed-brightness value; not used by the current display-off standby implementation |
| `DISPLAY_DIM_TIMEOUT_MS` | 10000 ms | display dim timeout |
| `RECORDER_HARDWARE_VERSION` | `1.00` | version text displayed on device |
| `RECORDER_SOFTWARE_VERSION` | `1.13` | version text displayed on device |
| `RECORDER_VERSION_TEXT` | `sw ver 1.13` / `hw ver 1.00` | main display version text |

### 3.3 Web/WiFi

| Configuration | Current value | Requirement use |
|---|---:|---|
| `AP_SSID_PREFIX` | `SLM-` | recorder WiFi access-point SSID prefix |
| `WEB_SERVER_PORT` | 80 | Web server port |
| `AP_IP_ADDRESS` | `192.168.4.1` | Web interface address |
| `AP_GATEWAY` | `192.168.4.1` | access-point gateway |
| `AP_SUBNET` | `255.255.255.0` | access-point subnet |
| `WEB_SINGLE_CLIENT_TIMEOUT_MS` | 60000 ms | Web single-client timeout |
| `WEB_SD_BUSY_STALE_MS` | 30000 ms | Web SD busy-lock stale recovery guard |

### 3.4 Calibration

| Configuration | Current value | Requirement use |
|---|---:|---|
| `CALIBRATION_PREFS_NAMESPACE` | `slm-cal` | calibration NVS namespace |
| `CALIBRATION_RECORD_VERSION` | 3 | calibration record version written in file block `0x72` |
| `CALIBRATION_SENSOR_STORAGE_VERSION` | 1 | sensor-calibration NVS storage schema version |
| `CALIBRATION_INSTALL_STORAGE_VERSION` | 1 | installation-calibration NVS storage schema version |
| `CALIBRATION_GRAVITY_MG` | 1000 mg | calibration reference gravity |
| `CALIBRATION_VALIDITY_MONTHS` | 12 months | calibration expiration |
| `CALIBRATION_FACE_GRAVITY_TOL_PCT` | 10% | face-axis gravity tolerance |
| `INSTALLATION_GRAVITY_TOL_PCT` | 10% | installation-calibration gravity magnitude tolerance |
| `CALIBRATION_SAMPLE_PERIOD_MS` | 50 ms | calibration sampling period |
| `CALIBRATION_WINDOW_SAMPLE_COUNT` | 40 samples | calibration stability window |
| `CALIBRATION_STABILITY_STDDEV_MAX_MG` | 1.5 mg | calibration stability threshold |
| `CALIBRATION_GAIN_MIN` | 0.8 | minimum acceptable calibration gain |
| `CALIBRATION_GAIN_MAX` | 1.2 | maximum acceptable calibration gain |
| `CALIBRATION_OFFSET_ABS_MAX_MG` | 200 mg | maximum absolute calibration offset |

### 3.5 Recording format

| Configuration | Current value | Requirement use |
|---|---:|---|
| `PACKET_SYNC_BYTE` | `0x55` | recording block synchronization byte |
| `PACKET_TYPE_ACCEL` | `0x70` | acceleration block identifier |
| `PACKET_TYPE_STATUS` | `0x71` | status/close block identifier |
| `PACKET_TYPE_CALIBRATION` | `0x72` | calibration block identifier |
| `FILENAME_MAX_LENGTH` | 64 | maximum generated daily filename/prefix length |

### 3.6 SD/storage

| Configuration | Current value | Requirement use |
|---|---:|---|
| `PREFS_NAMESPACE` | `slm-data` | settings NVS namespace |
| `SD_MAX_RECORD_FILES` | 50 files | recording file-count limit |
| `SD_RECORD_START_MIN_FREE_MB` | 500 MB | minimum free space required before recording start |
| `SD_RECORD_LOW_FREE_MB` | 250 MB | lower low-space threshold used while recording is active |
| `SD_IO_FAIL_LIMIT` | 3 | consecutive SD I/O failure limit |
| `SD_WRITE_RETRY_MAX` | 3 | SD write retry limit |
| `SD_RECORD_FLUSH_PERIOD_MS` | 500 ms | recording file flush period |
| `SD_TASK_PERIOD_MS` | 50 ms | normal SD task service period |
| `SD_TASK_FILE_OP_PERIOD_MS` | 1 ms | SD task service period while idle and Web file-management is authorized |
| `SD_IDLE_REPROBE_PERIOD_MS` | 500 ms | idle SD reprobe period |
| `SD_ERROR_REPROBE_PERIOD_MS` | 500 ms | SD error-state reprobe period |
| `SD_FILE_OP_TIMEOUT_MS` | 2000 ms | file-management operation timeout |
| `SD_MOUNT_PREFIX` | `/sdcard` | SD filesystem mount prefix |

### 3.7 State-machine timing and performance architecture

| Configuration | Current value | Requirement use |
|---|---:|---|
| `CFG_BOOT_TIMEOUT_MS` | 2000 ms | BOOT timeout |
| `CFG_STARTING_TIMEOUT_MS` | 1500 ms | STARTING/open timeout |
| `CFG_CLOSING_TIMEOUT_MS` | 1500 ms | STOPPING/close timeout |
| `CFG_POWERDOWN_DELAY_MS` | 1000 ms | normal delay before PMU shutdown request |
| `CFG_LOW_BATTERY_NOTICE_MS` | 10000 ms | low-battery recharge notice duration before PMU shutdown |
| `CFG_STATE_TASK_PERIOD_MS` | 50 ms | state/acquisition task period; supports 20 Hz recording |
| `CFG_STATE_HOUSEKEEPING_PERIOD_TICKS` | 20 | housekeeping period divisor |
| `CFG_RING_BUFFER_CAPACITY_ITEMS` | 128 | acquisition-to-SD buffer capacity |
| `CFG_STATE_TASK_CORE` | 0 | state/acquisition task core |
| `CFG_SD_TASK_CORE` | 1 | SD writing task core |
| `CFG_UI_TASK_CORE` | 1 | UI task core |
| `CFG_WEB_TASK_CORE` | 0 | Web task core |
| `ACCEL_READ_MAX_TRIES` | 3 | bounded acceleration read retries |
| `PMU_BATT_PCT_MAX_RETRIES` | 3 | PMU battery percentage read retries |
| `PMU_USB_MAX_RETRIES` | 3 | PMU USB status read retries |
| `TOUCH_READ_MAX_RETRIES` | 3 | touch read retries |
| `RTC_READ_MAX_RETRIES` | 3 | RTC read retries |
| `WATCHDOG_TIMEOUT_MS` | 3000 ms | software watchdog timeout |
| `WATCHDOG_CHECK_PERIOD_MS` | 1000 ms | watchdog checker period |
| `WATCHDOG_PREFS_NAMESPACE` | `slm-fault` | watchdog persistent-fault NVS namespace |
| `WATCHDOG_PREFS_KEY` | `wdg` | watchdog persistent-fault NVS key |


## 4. Operational Requirements

### 4.1 Device Power and Shutdown

#### OP-PWR-001 — Start by power button

The recorder shall be able to start when the operator presses the power/clear button.

Status:

- **Satisfied by PMU hardware.** This is not allocated to application software.

#### OP-PWR-002 — Start by USB power

The recorder shall be able to start when USB power is connected.

Status:

- **Satisfied by PMU hardware.** This is not allocated to application software.

#### OP-PWR-003 — Stop by power-button shutdown hold

When not recording and not blocked by a non-clearable error condition, the recorder shall stop when the operator holds the power/clear button for `POWER_SHUTDOWN_HOLD_MS`.

Status:

- **Implemented.**

#### OP-PWR-004 — Stop while recording by power-button shutdown hold

When recording is active, the recorder shall stop recording and close the recording file before shutting down when the operator holds the power/clear button for `POWER_SHUTDOWN_HOLD_MS`.

Status:

- **Implemented.**

#### OP-PWR-005 — Stop by USB removal while READY

When the recorder is in `ST_READY` and not blocked by a non-clearable error condition, the recorder shall stop when a fresh USB-present to USB-absent transition is detected.

USB absence that occurred before entering `ST_READY` shall not be remembered and shall not trigger shutdown after a recording is stopped. During `ST_RECORDING` and `ST_STOPPING`, USB removal shall not by itself request shutdown; the recorder shall continue on battery unless another shutdown condition exists.

Status:

- **Implemented.**

#### OP-PWR-006 — Stop on low-power condition

When running on battery, with no USB power present, the recorder shall stop from any state when a low-power condition occurs. Before PMU shutdown, the local display shall show a full-screen black low-battery notice with red text for 10 seconds: `BATTERY LOW` / `RECHARGE WITH USB`.

If a recording file is open or recording is in progress, the recorder shall close the recording file before shutdown. If no recording file is open, the recorder shall transition directly to shutdown.

Status:

- **Implemented.**

#### OP-PWR-007 — Forced unconditional shutdown

The recorder shall provide an unconditional forced shutdown when the power/clear button is held for the PMU/hardware forced-shutdown duration.

Status:

- **Satisfied by PMU hardware.**

#### OP-PWR-008 — Clear setup data gesture

When the recorder is READY, simultaneous holding of the record button and power/clear button until the clear gesture is accepted shall clear user settings and calibration NVS data, then stop the recorder.

Cleared data shall include:

- date/time configured flags;
- registration;
- WiFi password;
- stored calibration record;
- calibration fault latch.

Status:

- **Implemented.**

### 4.2 User Interface

#### OP-UI-COLOR-001 — Button color coding

The recorder UI shall use the following button color semantics:

| Color | Meaning |
|---|---|
| blue | active/available normal action |
| orange | user-resolvable action or condition that should be followed to resolve the current blocking condition |
| red | blocking fault/condition that cannot be resolved through the current device workflow |
| gray | inactive/unavailable action |
| green | back/return or positive navigation action |

Status:

- **Implemented.**

#### OP-UI-001 — Main display content

When started, the recorder shall display the main UI containing:

- time on top in large font, when available;
- date below time in medium font, when available;
- MENU button below date;
- hardware/software version text below MENU;
- message/error field at the bottom in large font;
- battery graphic at bottom-left.

Status:

- **Implemented.**

#### OP-UI-002 — Battery display

The battery graphic shall show four vertical cells, percentage indication, and a purple electric/charging symbol when USB power is present.

Battery cell colors shall be:

| Battery percent | Color |
|---:|---|
| 0-25% | red |
| 26-50% | orange |
| 51-75% | yellow |
| 76-100% | green |

Status:

- **Implemented.**

#### OP-UI-003 — Display standby

The display shall start at full brightness and enter standby after `DISPLAY_DIM_TIMEOUT_MS` without local operator interaction.

Standby display behavior shall be page-independent for normal recorder UI pages. It shall be allowed from the main page, MENU, SETTINGS, setting-edit pages, and WiFi-support pages. The active recorder message shall not by itself prevent standby. The dedicated low-battery shutdown notice is excluded because it intentionally displays mandatory red recharge instructions before PMU shutdown.

During RECORDING, date/time cache refresh is allowed while the display is active so the displayed clock continues to update. Recording sample timestamps remain based on the captured recording start time plus the monotonic ESP timer and do not depend on periodic RTC refresh.

When standby is active:

- the AMOLED display shall be switched off and appear black;
- the display panel supply controlled by `LCD_EN` shall be switched off;
- no standby text shall be displayed;
- normal UI refresh shall stop;
- minimal UI/LVGL/touch processing shall continue at a reduced rate to detect wake conditions, including while RECORDING.

The display shall restore the previously active page at full brightness when touch activity is detected. The display shall also wake on power/clear button activity, record button activity, or USB power insertion. These wake actions shall not clear settings, stop WiFi, start/stop recording, or acknowledge errors by themselves; normal button hold processing still applies separately.

Status:

- **Implemented.**

### 4.3 Menu and Web Access

#### OP-MENU-001 — Menu access while READY

When the recorder is READY and not recording, the MENU button shall be active and shall provide access to a secondary page with:

- START RECORD;
- START WIFI / STOP WIFI;
- SETTINGS;
- BACK.

The MENU button shall be:

- blue when READY and no setup/calibration/SD-maintenance action is required;
- orange when READY and a setup/calibration/SD-maintenance action is required;
- gray when inactive.

START WIFI shall be disabled while required settings are incomplete because the WiFi password is one of the required settings. Calibration and Firmware Update over the Web interface shall therefore only be offered after SETTINGS is complete.

The screen START RECORD action shall be disabled/gray while WiFi/Web access is active. The physical RECORD button remains available as an independent hardware recording control and may start recording when WiFi/Web is active; leaving READY for STARTING forces WiFi/Web OFF before recording.

The BACK button shall be green and always active.

Status:

- **Implemented.**

#### OP-MENU-002 — Menu access during setup lock

When the recorder is READY but recording is locked due to missing settings, missing/expired calibration, or calibration fault, MENU access shall remain available so the operator can complete setup or retry calibration.

During setup lock, orange buttons shall guide the operator toward the corrective action:

- if settings are required, MENU shall be orange and SETTINGS shall be orange;
- if calibration is missing, expired, or faulted, MENU shall be orange and START WIFI shall be orange;
- setting-specific buttons shall remain orange until their setting has been saved.

Status:

- **Implemented.**

#### OP-WEB-001 — WiFi/Web support

When START WIFI is selected, the recorder shall run a Web server at:

```text
http://192.168.4.1/
```

The Web server shall support file management, recorder calibration, firmware update, and a lightweight `/diag` health-check endpoint from a remote device connected to the recorder WiFi access point.

The Web listener shall be created, route-registered, and started once. Web ON/OFF shall control the WiFi access point and application-side access, but shall not call `AsyncWebServer::end()` during normal Web OFF. This is an implementation constraint of the selected AsyncWebServer/AsyncTCP stack: port-80 dispatch is not reliable after an `AsyncWebServer::end()` / later `begin()` lifecycle when HTTP traffic has occurred, even though AP, DHCP, and lower TCP layers remain functional.

Calibration access shall be protected because calibration is a maintenance/mechanical activity and not an everyday pilot action. The calibration menu and calibration action endpoints shall require the operator to enter the recorder registration string as the calibration password.

When calibration is required or faulted, the START WIFI button shall be orange to guide the operator to the Web calibration interface.

WiFi shall be turned OFF when the operator selects STOP WIFI or when the recorder leaves READY for a state transition such as recording start. The screen START RECORD action is disabled while WiFi is active; the physical RECORD button remains authoritative and can initiate recording, which forces WiFi/Web OFF through READY exit cleanup.

Status:

- **Implemented.**

#### OP-WEB-002 — Web unavailable during recording

Web/file-management support shall not interfere with active recording.

The persistent HTTP listener may remain allocated internally, but Web access is not operator-available during recording because the access point is off and SD file-management authorization is disabled.

Status:

- **Implemented.**

#### OP-WEB-003 — Web firmware update

The Web interface shall provide a Firmware Update function that allows the operator to upload a new firmware application binary through the recorder WiFi access point.

Firmware update shall require USB power to be present. If USB power is not detected, the update request shall be rejected.

The Firmware Update page shall instruct the operator to upload the recorder application `.bin` named like `SLM_recorder_date_version.bin`.

After a successful firmware update, the recorder shall restart automatically.

Status:

- **Implemented.**

### 4.4 Settings

#### OP-SET-001 — Required settings

The recorder shall require the following settings before recording is authorized:

- date;
- time;
- glider registration;
- WiFi password.

Status:

- **Implemented.**

#### OP-SET-002 — Settings persistence

Required settings shall be stored in non-volatile memory.

Status:

- **Implemented.**

#### OP-SET-003 — Settings pages

The SETTINGS page shall provide:

- DATE;
- TIME;
- REGISTRATION;
- WIFI PASSWORD;
- BACK.

Each setting page shall provide rollers to change the setting and shall provide:

- SAVE button at bottom-left;
- BACK button at bottom-right.

Rollers shall be preloaded with default values or with the values stored in NVS when available.

Status:

- **Implemented.**

#### OP-SET-004 — Settings button color state

Settings buttons shall follow the general UI color coding.

Required behavior:

- when settings are required, the main message shall show `NEED SETTINGS` in orange;
- MENU shall be orange to guide the operator to the menu;
- the SETTINGS button shall be orange while any required setting is missing;
- DATE, TIME, REGISTRATION, and WIFI PASSWORD buttons shall be orange until the respective setting has been saved;
- each setting-specific button shall turn blue after its setting has been saved;
- the SETTINGS button shall turn blue when all four required settings have been saved.

Status:

- **Implemented.**

#### OP-SET-006 — Settings before calibration/Web access

When settings are incomplete, the recorder shall display `NEED SETTINGS` in preference to calibration-required messages.

The START WIFI action shall be disabled while required settings are incomplete because the WiFi password is one of the required settings. The operator shall complete SETTINGS before Web calibration can be started.

Status:

- **Implemented.**

### 4.5 Calibration

#### OP-CAL-001 — Calibration required before recording

The recorder shall not authorize recording unless a valid, non-expired calibration exists.

Status:

- **Implemented.**

#### OP-CAL-002 — Calibration through Web interface

The recorder shall provide a Web interface allowing the operator to perform calibration while the recorder is not recording. The Calibration tab shall first request the recorder registration as a calibration password, then show a simple calibration menu with separate Accelerometer Calibration and Installation Calibration buttons. Each button shall display the date of the last saved calibration of that type when available. Calibration pages shall show simple live progress so the operator can see that sampling continues even when the best candidate is not improving.

Status:

- **Implemented.**

#### OP-CAL-003 — Six-face calibration

Calibration shall use six recorder orientations:

```text
+X, -X, +Y, -Y, +Z, -Z
```

The operator may present the faces in any order.

Status:

- **Implemented.**

#### OP-CAL-004 — Automatic calibration capture

During calibration, the recorder shall automatically capture stable face values without requiring the operator to manually accept each face. The current session shall retain the best capture for each face using the dominant face-axis standard deviation as the replacement metric. A valid capture shall not reset the rolling window; the recorder shall continue to evaluate overlapping windows while the face remains stable.

Status:

- **Implemented.**

#### OP-CAL-005 — Calibration review and save

The operator shall be able to review current calibration results and stored NVS calibration values before saving the accepted calibration. The accelerometer page shall use a simplified live progress area showing validity status with NVS date when valid, session state, current face, samples processed on that face, lowest stddev for that face, current-face update count, time since the last best update, and a compact six-face completion summary. The accelerometer workflow text shall instruct the operator to start calibration, place the recorder still on each of its six faces, wait for each face to show OK, leave the recorder on a given face until the last best update is more than 10 seconds old when practical, and save calibration when all six face values are satisfactory. The face summary shall show unprocessed faces in plain text, the active face in amber only until processed, and processed faces in green. The installation page shall show validity status with NVS date when valid, session state, samples processed, lowest noise, update count, time since the last best update, and the candidate or stored installation matrix. The installation workflow text shall direct the operator to put the glider in flight-level attitude with wings leveled following the AMM procedure, confirm sensor calibration is already valid, start calibration, leave the glider still, wait until the last best update is more than 10 seconds old when practical, and save calibration when noise is satisfactory.

Status:

- **Implemented.**

#### OP-CAL-006 — Calibration fault recovery

If calibration fails plausibility checks, the recorder shall retain the stored valid calibration. The operator shall be able to retry calibration before rejecting the recorder.

Status:

- **Implemented.**

### 4.6 Recording Authorization and Operation

#### OP-REC-001 — Recording start action

Recording shall be started by holding the physical record/start-stop button for `RECORD_START_HOLD_MS`. The local MENU screen also provides a START RECORD button using its own hold time; that screen button shall be disabled while WiFi/Web is active.

Status:

- **Implemented.**

#### OP-REC-002 — Recording start authorization

Recording shall start only when all required operating conditions are satisfied:

- recorder is READY;
- no active blocking error;
- SD card is present;
- SD free space is above the recording-start threshold;
- SD file count is below `SD_MAX_RECORD_FILES`;
- required settings are saved;
- valid calibration exists;
- calibration is not expired;
- a hardware record-button start hold is qualified, or a permitted UI START RECORD hold is qualified.

The physical RECORD button is intentionally independent of the UI/Web layer. If WiFi/Web is active and the physical RECORD button starts recording, the recorder shall turn WiFi/Web OFF as part of leaving READY. The UI START RECORD button shall not start recording while WiFi/Web is active.

Status:

- **Implemented.**

#### OP-STOP-001 — Stop by record button

Recording shall stop when the physical record/start-stop button is held for `RECORD_STOP_HOLD_MS`.

Status:

- **Implemented.**

#### OP-STOP-002 — Stop by power button

Recording shall stop and the SD file shall close before shutdown when the power/clear button is held for `POWER_SHUTDOWN_HOLD_MS`.

Status:

- **Implemented.**

#### OP-STOP-003 — Stop by low power

Recording shall stop and the SD file shall close before shutdown when a low-power condition occurs while running on battery with no USB power present. After the file is closed, the recorder shall show the 10-second `BATTERY LOW` / `RECHARGE WITH USB` notice before PMU shutdown.

Status:

- **Implemented.**

### 4.7 SD and File Management

#### OP-SD-001 — SD card required

Recording shall not be authorized without an SD card.

Status:

- **Implemented.**

#### OP-SD-002 — SD free space required before recording

Recording shall not start if SD free space is below the recording-start threshold:

```text
SD_RECORD_START_MIN_FREE_MB = 500 MB
```

Because file archive moves files to `/processed` on the same SD card and does not free memory, this condition cannot be corrected by Web archive.

Status:

- **Implemented.**

#### OP-SD-003 — SD low space while recording

If SD free space falls below the lower in-recording threshold:

```text
SD_RECORD_LOW_FREE_MB = 250 MB
```

while recording, the recorder shall close the file through the normal close path before reporting the low-space condition.

Status:

- **Implemented.**


The recording-start threshold shall be higher than the in-recording low-space threshold so that a recording does not start just above the low-space limit and immediately stop with `SD LOW`.

#### OP-SD-004 — SD file-count limit

Recording shall not start if the number of recording files on SD reaches or exceeds:

```text
SD_MAX_RECORD_FILES = 50
```

When this condition exists while not recording, and SD free space is still above the low-space threshold, the recorder shall remain in READY and allow MENU / START WIFI access so the operator can archive root files through Web file management.

The SD file-count maintenance message shall not force the UI back to the main page. `MSG_SD_FULL_FILES` shall be handled like setup-lock messages for local navigation: recording remains blocked, but MENU and START WIFI remain usable.

If another SD recovery path reclassifies into `ERR_SD_FILES_FULL`, the recorder shall leave ERROR and return to READY so file-count maintenance can be performed from the Web interface.

The `SD FULL (FILES)` message shall be orange because the condition is user-resolvable through the Web archive workflow.

Status:

- **Implemented.**

#### OP-SD-005 — Web file management

When WiFi is ON and a remote device connects to:

```text
http://192.168.4.1/
```

the Web file-management page shall show:

- SD size;
- SD remaining space;
- battery charge percentage.

The page shall allow the operator to:

- visualize/list files on SD;
- download files from SD;
- perform local browser-side flight-time analysis during download;
- display detected flight times and sample-period average/standard deviation;
- archive root recording files to `/processed` when Archive is requested;
- permanently delete selected files already in `/processed` from the Maintenance / Delete page.

Web file management shall be accessible when recording is not active, including when recording is blocked by SD max-file-count maintenance while SD free space is still available.

Status:

- **Implemented.**


#### OP-SD-006 — Web file archive instead of deletion

When the operator requests file deletion from the Web interface, the recorder shall preserve the file by moving it from the SD root to:

```text
/processed
```

If `/processed` does not exist, it shall be created.

If a file with the same name already exists in `/processed`, the recorder shall create a unique destination name by appending `_N` before the file extension.

Example:

```text
/FCFAG_20260517_222418.bin
/processed/FCFAG_20260517_222418.bin
/processed/FCFAG_20260517_222418_1.bin
```

The file shall be removed from the active root-file list only after the move succeeds.

#### OP-SD-007 — Daily recording file policy

For a given registration and UTC/local RTC date token, the recorder shall store all recording sessions of that day in one root-level daily recording file.

The daily filename pattern shall be:

```text
/REGISTRATION_YYYYMMDD_N.bin
```

where `N` is the number of recording sessions that have been started in that daily file.

On the first session of the day, `N` shall be `1`. For each subsequent session on the same day, the recorder shall rename the existing matching daily file from `_N.bin` to `_(N+1).bin`, then open it in append mode and write the session data to the end of the same file.

If more than one root file matches the same registration/date daily prefix, the recorder shall treat this as an SD fault rather than guessing which file should be appended.

Only root-level files shall be considered for daily append/rename matching. Files in `/processed` or any other subdirectory shall not be selected, even if their basename matches the daily filename pattern for the same registration and date. Files that do not match the daily filename pattern shall not be selected for append/rename matching.

Status:

- **Implemented.**

### 4.8 Recording File Content and Format

#### OP-FILE-001 — Recording file acceleration block

Recording files shall contain acceleration data acquired during recording.

Packet/block type:

```text
0x70
```

Status:

- **Implemented.**

#### OP-FILE-002 — Recording file status block

Recording files shall include the recording status/close information.

Packet/block type:

```text
0x71
```

Status:

- **Implemented.**

#### OP-FILE-003 — Recording file calibration block

Recording files shall include the active calibration data before acceleration sample blocks are written.

Packet/block type:

```text
0x72
```

Status:

- **Implemented and validated by recorded-file inspection.**

#### OP-FILE-004 — Recording block definitions

The recording format shall use the block definitions in Section 8 of this document.

Status:

- **Implemented.**

### 4.9 Performance and Timing

#### OP-PERF-001 — Recording sample rate

The recorder shall acquire and record acceleration data at:

```text
20 Hz
```

Status:

- **Implemented and validated on recorded file `FCFAG_20260517_222418.bin`.**

Hardware configuration note:

- The QMI8658 accelerometer is configured at ±8 g range and 1000 Hz hardware output data rate with hardware LPF mode 0. The 20 Hz value above is the application-level acquisition/recording cadence.

#### OP-PERF-002 — Acceleration coding

Recorded acceleration values shall be coded as signed 16-bit integers.

Status:

- **Implemented.**

#### OP-PERF-003 — Acquisition jitter minimization

The recorder shall minimize acquisition timing jitter. The acquisition path shall be designed so SD-card write latency does not directly block the acquisition timing.

Rationale and implementation:

- acquisition/state timing and SD writing run on different ESP32-S3 cores;
- a ring/circular buffer decouples acquisition from SD writing;
- validation file `FCFAG_20260517_222418.bin` showed exact 50 ms timestamp intervals and 0 ms interval standard deviation for that run.

Status:

- **Implemented and validated by recorded-file inspection.**

### 4.10 Message and Error Display

#### OP-MSG-001 — Blocking-condition messages

When an operator action cannot proceed because a setup, calibration, SD, power, or hardware condition blocks operation, the recorder shall display a user-visible message identifying the blocking condition.

The message and error triggering list is provided in Section 7.

Status:

- **Implemented.**

### 4.11 Software Watchdog

#### OP-WDG-001 — Software watchdog supervision

The recorder shall monitor critical task progress using software watchdog
heartbeats.

Monitored sources:

```text
state_task: required continuously
sd_task:    required continuously
recording:  required only while RECORDING
```

Each required source shall refresh its heartbeat before `WATCHDOG_TIMEOUT_MS`
expires. The watchdog check is performed from the Arduino `.ino` loop every
`WATCHDOG_CHECK_PERIOD_MS`.

If a watchdog timeout occurs, the recorder shall:

1. store a persistent watchdog fault flag in NVS;
2. if recording is active, request a controlled SD close;
3. wait up to `WATCHDOG_TIMEOUT_MS` for the close to complete;
4. request PMU shutdown.

On startup, after the UI and state-task local services are initialized, the
recorder shall display `FATAL WDG/CLR` in red before normal BOOT checks
continue. Pressing the power/clear button shall clear the persistent watchdog
flag and allow normal BOOT checks to continue. The watchdog fault is an operator acknowledgement latch, not
a permanent recording block.

## 5. Requirement Implementation Allocation

| Operational requirement | Primary code allocation | Validation candidates |
|---|---|---|
| OP-PWR-001 | PMU hardware, boot environment | hardware power validation |
| OP-PWR-002 | PMU hardware, boot environment | hardware power validation |
| OP-PWR-003 | `state_task` | manual power test |
| OP-PWR-004 | `state_task`, `sd_task` | VAL-REC-003 |
| OP-PWR-005 | `state_task` | manual power test |
| OP-PWR-006 | `state_task`, `sd_task` | low-power review/test |
| OP-PWR-007 | PMU hardware | hardware forced-shutdown validation |
| OP-PWR-008 | `state_task`, `settings_store`, `calibration_service`, `calibration_store` | manual setup-clear test |
| OP-UI-COLOR-001 | `ui_task`, `ui_helpers` | visual/UI check |
| OP-UI-001 | `ui_task`, `ui_helpers`, `ui_message` | VAL-UI-001 |
| OP-UI-002 | `ui_helpers` | VAL-UI-001 |
| OP-UI-003 | `ui_task`, `display_driver`, `button_hold_helpers` | manual UI/standby test |
| OP-MENU-001 | `ui_task` | VAL-WIFI-001 |
| OP-MENU-002 | `ui_task`, `ui_message`, `state_task` | VAL-CAL-001 |
| OP-WEB-001 | `web_task`, `html_interface`, `ui_task` | VAL-WIFI-001 |
| OP-WEB-002 | `state_task`, `web_task`, `sd_files` | VAL-WIFI-001 |
| OP-WEB-003 | `web_task`, `html_interface`, ESP32 `Update` API, `partitions.csv` | VAL-OTA-001 |
| OP-SET-001 | `settings_store`, `state_task` | VAL-SET-001 |
| OP-SET-002 | `settings_store` | VAL-SET-001 |
| OP-SET-003 | `ui_task`, `settings_store` | VAL-SET-001 |
| OP-SET-004 | `ui_task`, `ui_helpers` | visual/UI check |
| OP-CAL-001 | `calibration_service`, `calibration_store`, `state_task` | VAL-CAL-001, VAL-CAL-004 |
| OP-CAL-002 | `web_task`, `html_interface`, `calibration_service` | VAL-CAL-002 |
| OP-CAL-003 | `calibration_service` | VAL-CAL-002 |
| OP-CAL-004 | `calibration_service`, `state_task` | VAL-CAL-002 |
| OP-CAL-005 | `calibration_service`, `web_task`, `html_interface` | VAL-CAL-003 |
| OP-CAL-006 | `calibration_service`, `calibration_store` | VAL-CAL-006 |
| OP-REC-001 | `state_task`, `button_hold_helpers` | VAL-REC-001 |
| OP-REC-002 | `state_task`, `settings_store`, `calibration_service`, `sd_task` | VAL-REC-001 |
| OP-STOP-001 | `state_task` | VAL-REC-002 |
| OP-STOP-002 | `state_task`, `sd_task` | VAL-REC-003 |
| OP-STOP-003 | `state_task`, `sd_task` | low-power review/test |
| OP-SD-001 | `sd_task`, `state_task` | VAL-SD-001 |
| OP-SD-002 | `sd_task`, `sd_storage` | VAL-SD-003 |
| OP-SD-003 | `sd_task` | VAL-SD-003 |
| OP-SD-004 | `sd_task`, `sd_storage` | VAL-SD-003 |
| OP-SD-005 | `web_task`, `sd_files`, `sd_storage` | VAL-WIFI-001 |
| OP-SD-006 | `web_task`, `sd_files`, `sd_storage` | Web file archive validation |
| OP-SD-007 | `record_format`, `sd_task`, `sd_storage` | daily recording file validation |
| OP-FILE-001 | `record_format`, `state_task`, `sd_task`, `ring_buffer` | output file inspection |
| OP-FILE-002 | `record_format`, `sd_task` | output file inspection |
| OP-FILE-003 | `record_format`, `sd_task`, `calibration_service` | output file inspection |
| OP-FILE-004 | `record_format` | output file inspection |
| OP-PERF-001 | `state_task`, `ring_buffer`, `sd_task` | VAL-PERF-001 |
| OP-PERF-002 | `record_format` | output file inspection |
| OP-PERF-003 | `ring_buffer`, `state_task`, `sd_task`, FreeRTOS core allocation | VAL-PERF-002 |
| OP-MSG-001 | `state_task`, `error_manager`, `ui_message` | message/error validation |
| OP-WDG-001 | `watchdog_service`, `SLM_recorder.ino`, `state_task`, `sd_task` | watchdog source review and manual fault review |

## 6. Recorded-File Validation Evidence

Recorded-file validation evidence:

```text
file: FCFAG_20260517_222418.bin
bytes: 13909
blocks: 0x72=1, 0x70=1054, 0x71=1
calibration timestamp: 2026-05-17 14:05:31
gain: x=1.000363, y=1.000100, z=1.000200
offset_mg: x=3.0, y=-63.3, z=-11.7
accel intervals:
  count=1053
  avg_ms=50.000
  min_ms=50
  max_ms=50
  stddev_ms=0.000
  derived_rate_hz=20.000
result: PASS
```

## 7. Message and Error Display Policy

### 7.1 Display Message Table

The table below lists messages rendered in the normal bottom message area of the local UI.

| Display message | Color | Type | Recoverable / operator action |
|---|---|---|---|
| `BOOT` | Green | startup | not an error |
| `READY` | Green | normal operation | not an error |
| `RECORDING` | Green | normal operation | not an error |
| `STARTING` | Green | transient | not an error |
| `STOPPING` | Green | transient | not an error |
| `SHUTDOWN` | Green | shutdown | normal operator-requested shutdown transition; not an error |
| `SD OK/CLR` | Green | SD recovered / clear prompt | yes, press clear after the SD condition is gone |
| `NEED SETTINGS` | Amber | setup required | yes, perform date, time, registration, and password settings |
| `ACC CAL REQ` | Amber | accelerometer calibration required | yes, perform password-protected Web accelerometer calibration menu (`START WIFI`) |
| `INST CAL REQ` | Amber | installation calibration required | yes, perform password-protected Web installation calibration menu (`START WIFI`) |
| `NO SD` | Amber | SD/storage warning | yes, insert SD card |
| `SD LOW` | Amber | SD/storage warning | yes, insert an SD card with enough free space; archive alone does not free SD space |
| `SD FULL (FILES)` | Amber | SD file-count maintenance | yes, archive root files and/or delete files from `/processed` using recorder Web interface (`START WIFI`) |
| `LOW BATT` | Amber | power warning | yes, connect USB power |
| `CAL FAULT` | Red | calibration fault | possible, reset if required and repeat settings/calibrations |
| `ACCEL ERR` | Red | hardware/runtime error | possible, press the power/clear button up to 8 seconds to stop recorder, then restart * |
| `RTC ERROR` | Red | hardware/runtime error | possible, press the power/clear button up to 8 seconds to stop recorder, then restart * |
| `PMU ERROR` | Red | hardware/runtime error | possible, press the power/clear button up to 8 seconds to stop recorder, then restart * |
| `REC FAIL` | Red | recording failure | possible, press the power/clear button up to 8 seconds to stop recorder, then restart * |
| `TOUCH ERROR` | Red | hardware/runtime error | possible, press the power/clear button up to 8 seconds to stop recorder, then restart * |
| `ERROR` | Red | generic fallback error | possible, press the power/clear button up to 8 seconds to stop recorder, then restart * |
| `SD ERROR` | Red | SD/storage fault | possible, press the power/clear button up to 8 seconds to stop recorder, then restart * |
| `GENERIC ERROR` | Red | fatal error | possible, press the power/clear button up to 8 seconds to stop recorder, then restart * |
| `FATAL WDG/CLR` | Red | watchdog fault acknowledgement | possible, press the power/clear button to acknowledge the watchdog fault; if required, hold power/clear up to 8 seconds to stop recorder, then restart * |

\* If the error persists after restart, report the problem to support.

### 7.1.1 Special Full-Screen Displays

The following screens are not normal bottom message-area messages.

| Screen text | Color | Trigger / operator action |
|---|---|---|
| Display off / black screen | No text displayed | display standby; touch screen, press a hardware button, or connect USB power to wake |
| `BATTERY LOW` / `RECHARGE WITH USB` | Red on black | low-battery shutdown notice; connect USB power; displayed for 10 seconds before PMU shutdown |

### 7.2 Error and Trigger Mapping

| Condition / trigger | Error code | Display message | Triggering logic / source |
|---|---|---|---|
| Settings incomplete while READY | none | `NEED SETTINGS` | `state_task` checks `settings_store` completeness |
| Sensor calibration missing or expired while READY | none | `ACC CAL REQ` | `calibration_service` status consumed by `state_task` |
| Installation calibration missing while READY | none | `INST CAL REQ` | `calibration_service` installation validity consumed by `state_task` |
| Calibration plausibility fault | `ERR_CALIBRATION_FAULT` | `CAL FAULT` | `calibration_service` / `calibration_store` |
| SD card missing | `ERR_SD_NO_CARD` | `NO SD`, then `SD OK/CLR` when recovered | `sd_task` status / recovery path |
| SD free space below recording-start threshold or below in-recording low-space threshold | `ERR_SD_SPACE_LOW` | `SD LOW` | `sd_task` pre-open check or low-space during write |
| SD file-count threshold reached | `ERR_SD_FILES_FULL` | `SD FULL (FILES)` | `sd_task` status check; message allows Web maintenance |
| Unexpected SD I/O fault | `ERR_SD_FAULT` | `SD ERROR` | `sd_task` write/flush/open/close failure classification |
| Accelerometer read failure | `ERR_ACCEL_NO_RESPONSE` | `ACCEL ERR` | `state_task` recording acquisition retries exhausted |
| Ring-buffer overflow | `ERR_RINGBUFFER_OVERFLOW` | `REC FAIL` | `ring_buffer_push()` failure during recording |
| RTC invalid/fault | `ERR_RTC_INVALID` | `RTC ERROR` | RTC/date-time validation path |
| PMU fault | `ERR_PMU_FAULT` | `PMU ERROR` | PMU/power status path |
| Touch fault | `ERR_TOUCH_FAULT` | `TOUCH ERROR` | touch driver/service path |
| Low battery warning | none | `LOW BATT` or dedicated shutdown notice | power/battery status consumed by `state_task` |
| USB lost while READY | none | shutdown path, no dedicated display message | USB loss edge consumed by `state_task` |
| Shutdown requested | none | `SHUTDOWN` | state transition to OFF |
| Persistent watchdog fault at boot | watchdog NVS latch | `FATAL WDG/CLR` | startup watchdog-fault check |

Notes:

- Green messages are normal or confirmation states.
- Amber messages indicate a recoverable setup, calibration, SD, or power condition.
- Red messages indicate faults. The operator may try to stop/restart the recorder; if the error persists after restart, support action is required.
- SD recoverable errors use a two-step recovery: the physical condition must be corrected, then the operator acknowledges `SD OK/CLR`.
- Setup-lock messages are not active errors; they keep the recorder in READY and allow MENU access.

## 8. Recording Block Definitions

### 8.1 Block Summary

| Block ID | Name | Size | Status |
|---:|---|---:|---|
| `0x70` | Acceleration block | 13 bytes | implemented |
| `0x71` | Status/close block | 13 bytes | implemented |
| `0x72` | Calibration block | 252 bytes | implemented and validated |

All blocks start with:

```text
sync = 0x55
```

The checksum is the lower 8 bits of the sum of all bytes in the block except the checksum byte itself.

### 8.2 Acceleration Block `0x70`

| Offset | Type | Field | Description |
|---:|---|---|---|
| 0 | `uint8` | sync | `0x55` |
| 1 | `uint8` | id | `0x70` |
| 2 | `int32` | ts_ms | sample timestamp in milliseconds |
| 6 | `int16` | ax | corrected X acceleration, milli-g |
| 8 | `int16` | ay | corrected Y acceleration, milli-g |
| 10 | `int16` | az | corrected Z acceleration, milli-g |
| 12 | `uint8` | checksum | checksum over bytes 0-11 |

### 8.3 Status Block `0x71`

| Offset | Type | Field | Description |
|---:|---|---|---|
| 0 | `uint8` | sync | `0x55` |
| 1 | `uint8` | id | `0x71` |
| 2 | `uint16` | overflow | ring-buffer overflow count, saturated to `0xFFFF` |
| 4 | `uint16` | reserved1 | reserved |
| 6 | `uint16` | reserved2 | reserved |
| 8 | `uint16` | reserved3 | reserved |
| 10 | `uint16` | reserved4 | reserved |
| 12 | `uint8` | checksum | checksum over bytes 0-11 |

### 8.4 Calibration Block `0x72`

Placement:

- immediately after the recording file is opened;
- before the first acceleration block.

| Offset | Type | Field | Description |
|---:|---|---|---|
| 0 | `uint8` | sync | `0x55` |
| 1 | `uint8` | id | `0x72` |
| 2 | `uint16` | size | block size, currently `252` |
| 4 | `uint32` | calibration_version | calibration record version |
| 8 | `uint16` | year | sensor calibration year |
| 10 | `uint8` | month | sensor calibration month |
| 11 | `uint8` | day | sensor calibration day |
| 12 | `uint8` | hour | sensor calibration hour |
| 13 | `uint8` | minute | sensor calibration minute |
| 14 | `uint8` | second | sensor calibration second |
| 15 | `float32[3]` | gain_x/y/z | sensor calibration gains |
| 27 | `float32[3]` | offset_x/y/z_mg | sensor calibration offsets, milli-g |
| 39 | `float32[6][3]` | face_mean_mg | raw face mean values, milli-g |
| 111 | `float32[6][3]` | face_stddev_mg | raw face standard deviations, milli-g |
| 183 | `uint8` | installation_valid | `1` when installation calibration is valid, otherwise `0` |
| 184 | `uint16` | installation_year | installation calibration year |
| 186 | `uint8` | installation_month | installation calibration month |
| 187 | `uint8` | installation_day | installation calibration day |
| 188 | `uint8` | installation_hour | installation calibration hour |
| 189 | `uint8` | installation_minute | installation calibration minute |
| 190 | `uint8` | installation_second | installation calibration second |
| 191 | `float32[3]` | installation_mean_mg | installation calibration mean gravity vector, milli-g |
| 203 | `float32[3]` | installation_stddev_mg | installation calibration standard deviation vector, milli-g |
| 215 | `float32[9]` | installation_matrix | row-major 3 x 3 installation rotation matrix |
| 251 | `uint8` | checksum | checksum over bytes 0-250 |

Face order:

```text
0 = +X
1 = -X
2 = +Y
3 = -Y
4 = +Z
5 = -Z
```

Axis order inside each face vector:

```text
0 = X
1 = Y
2 = Z
```

Notes:

- `gravity_mg` is not stored because the reference value is fixed by the calibration algorithm/configuration.
- `face_valid` is not stored because recording can only start after all six faces have been captured and a valid calibration has been saved.

### 8.5 Recording File Naming and Session Sequence

Recording files use a daily-file policy. `record_format` builds the daily prefix from registration and date only:

```text
/REGISTRATION_YYYYMMDD
```

`sd_storage` appends the session suffix and extension. The first session of the day uses:

```text
/REGISTRATION_YYYYMMDD_1.bin
```

For each subsequent recording session on the same day, the existing root-level file is renamed to the next suffix, for example `_2.bin`, and opened in append mode. The suffix is the daily session count. Archived files under `/processed` are ignored and are never selected as the active daily file for append.

Each session appended to the daily file has the normal session block sequence:

```text
0x72 calibration block
0x70 acceleration block
0x70 acceleration block
...
0x71 status block
```

## 9. Remaining Implementation / Review Items

No open implementation gaps are currently listed in this document.

Items to continue monitoring:

| Item | Status |
|---|---|
| Final acceptance tolerance for 20 Hz timing/jitter | validation method exists; acceptance tolerance can still be formalized |
| Recorded-file validation for binary format revisions | required for each block-format revision |
| Daily recording file behavior | validate first session creates `_1.bin`; second same-day session renames/appends to `_2.bin`; archived same-day files under `/processed` and non-daily-pattern files are ignored for append/rename matching |




