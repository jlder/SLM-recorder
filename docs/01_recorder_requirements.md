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
```

The firmware shall not require these files to be manually copied into the global Arduino libraries folder.

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
| `DISPLAY_BRIGHTNESS_DIMMED` | 60 | standby display brightness |
| `DISPLAY_DIM_TIMEOUT_MS` | 10000 ms | display dim timeout |
| `RECORDER_HARDWARE_VERSION` | `1.00` | version text displayed on device |
| `RECORDER_SOFTWARE_VERSION` | `1.00` | version text displayed on device |
| `RECORDER_VERSION_TEXT` | `ver 1.00/1.00` | main display version text |

### 3.3 Web/WiFi

| Configuration | Current value | Requirement use |
|---|---:|---|
| `AP_SSID_PREFIX` | `SLM-` | recorder WiFi access-point SSID prefix |
| `WEB_SERVER_PORT` | 80 | Web server port |
| `AP_IP_ADDRESS` | `192.168.4.1` | Web interface address |
| `AP_GATEWAY` | `192.168.4.1` | access-point gateway |
| `AP_SUBNET` | `255.255.255.0` | access-point subnet |
| `WEB_SINGLE_CLIENT_TIMEOUT_MS` | 60000 ms | Web single-client timeout |

### 3.4 Calibration

| Configuration | Current value | Requirement use |
|---|---:|---|
| `CALIBRATION_PREFS_NAMESPACE` | `slm-cal` | calibration NVS namespace |
| `CALIBRATION_RECORD_VERSION` | 1 | calibration record version |
| `CALIBRATION_GRAVITY_MG` | 1000 mg | calibration reference gravity |
| `CALIBRATION_VALIDITY_MONTHS` | 12 months | calibration expiration |
| `CALIBRATION_FACE_GRAVITY_TOL_PCT` | 10% | face-axis gravity tolerance |
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
| `FILENAME_MAX_LENGTH` | 64 | maximum generated filename length |

### 3.6 SD/storage

| Configuration | Current value | Requirement use |
|---|---:|---|
| `PREFS_NAMESPACE` | `slm-data` | settings NVS namespace |
| `SD_MAX_RECORD_FILES` | 50 files | recording file-count limit |
| `SD_SPACE_LOW_MB` | 500 MB | minimum free-space threshold before/during recording |
| `SD_IO_FAIL_LIMIT` | 3 | consecutive SD I/O failure limit |
| `SD_WRITE_RETRY_MAX` | 3 | SD write retry limit |
| `SD_RECORD_FLUSH_PERIOD_MS` | 500 ms | recording file flush period |
| `SD_TASK_PERIOD_MS` | 50 ms | SD task service period |
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
| `CFG_POWERDOWN_DELAY_MS` | 1000 ms | delay before PMU shutdown request |
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

#### OP-PWR-005 — Stop by USB removal while idle

When not recording and not blocked by a non-clearable error condition, the recorder shall stop when USB power is removed.

Status:

- **Implemented in READY.**

#### OP-PWR-006 — Stop on low-power condition

When running on battery, with no USB power present, the recorder shall stop from any state when a low-power condition occurs.

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

The display shall start at full brightness and enter standby after `DISPLAY_DIM_TIMEOUT_MS`.

Standby display behavior shall be allowed only while the recorder state is READY or RECORDING and the active UI page is the main display. The normal `READY` and `RECORDING` status messages shall not prevent standby.

Standby shall not start while the operator is on MENU, SETTINGS, setting-edit pages, or Web/WiFi support pages shown on the local display.

During RECORDING, date/time cache refresh is allowed while the display is active so the displayed clock continues to update. Recording sample timestamps remain based on the captured recording start time plus the monotonic ESP timer and do not depend on periodic RTC refresh.

When standby is active:

- the standby screen shall have a black background;
- the standby screen shall show large dimmed white text reading `TOUCH TO ACTIVATE`;
- normal UI refresh shall stop;
- minimal UI/touch processing shall continue at a reduced rate to detect wake conditions, including while RECORDING.

The display shall return to the active main UI at full brightness when:

- touch activity is detected;
- a setup, fault, or error message is active;
- the power/clear button is pressed;
- the record button is pressed;
- USB power is inserted;
- recorder state leaves READY or RECORDING.

SD insertion/removal does not need a separate brightness trigger because SD insertion/removal conditions generate user-visible messages.

Status:

- **Implemented.**

### 4.3 Menu and Web Access

#### OP-MENU-001 — Menu access while READY

When the recorder is READY and not recording, the MENU button shall be active and shall provide access to a secondary page with:

- START WIFI;
- SETTINGS;
- BACK.

The MENU button shall be:

- blue when READY and no setup/calibration action is required;
- orange when READY and a setup/calibration action is required;
- gray when inactive.

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

The Web server shall support file management and recorder calibration from a remote device connected to the recorder WiFi access point.

When calibration is required or faulted, the START WIFI button shall be orange to guide the operator to the Web calibration interface.

WiFi shall be turned OFF when the operator leaves the MENU page using BACK to return to the main page.

Status:

- **Implemented.**

#### OP-WEB-002 — Web unavailable during recording

Web/file-management support shall not interfere with active recording.

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

### 4.5 Calibration

#### OP-CAL-001 — Calibration required before recording

The recorder shall not authorize recording unless a valid, non-expired calibration exists.

Status:

- **Implemented.**

#### OP-CAL-002 — Calibration through Web interface

The recorder shall provide a Web interface allowing the operator to perform calibration while the recorder is not recording.

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

During calibration, the recorder shall automatically capture stable face values without requiring the operator to manually accept each face.

Status:

- **Implemented.**

#### OP-CAL-005 — Calibration review and save

The operator shall be able to review current calibration results and stored NVS calibration values before saving the new calibration.

Status:

- **Implemented.**

#### OP-CAL-006 — Calibration fault recovery

If calibration fails plausibility checks, the recorder shall not overwrite the previous valid calibration. The operator shall be able to retry calibration before rejecting the recorder.

Status:

- **Implemented.**

### 4.6 Recording Authorization and Operation

#### OP-REC-001 — Recording start action

Recording shall be started by holding the physical record/start-stop button for `RECORD_START_HOLD_MS`.

Status:

- **Implemented.**

#### OP-REC-002 — Recording start authorization

Recording shall start only when all required operating conditions are satisfied:

- recorder is READY;
- no active blocking error;
- SD card is present;
- SD free space is sufficient;
- SD file count is below `SD_MAX_RECORD_FILES`;
- required settings are saved;
- valid calibration exists;
- calibration is not expired;
- record-button start hold is qualified.

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

Recording shall stop and the SD file shall close before shutdown when low-power condition occurs while running on battery with no USB power present.

Status:

- **Implemented.**

### 4.7 SD and File Management

#### OP-SD-001 — SD card required

Recording shall not be authorized without an SD card.

Status:

- **Implemented.**

#### OP-SD-002 — SD free space required before recording

Recording shall not start if SD free space is below:

```text
SD_SPACE_LOW_MB = 500 MB
```

Because file archive moves files to `/processed` on the same SD card and does not free memory, this condition cannot be corrected by Web archive.

Status:

- **Implemented.**

#### OP-SD-003 — SD low space while recording

If SD free space falls below:

```text
SD_SPACE_LOW_MB = 500 MB
```

while recording, the recorder shall close the file through the normal close path before reporting the low-space condition.

Status:

- **Implemented.**

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
- archive files to `/processed` when delete is requested.

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
| OP-FILE-001 | `record_format`, `state_task`, `sd_task`, `ring_buffer` | output file inspection |
| OP-FILE-002 | `record_format`, `sd_task` | output file inspection |
| OP-FILE-003 | `record_format`, `sd_task`, `calibration_service` | output file inspection |
| OP-FILE-004 | `record_format` | output file inspection |
| OP-PERF-001 | `state_task`, `ring_buffer`, `sd_task` | VAL-PERF-001 |
| OP-PERF-002 | `record_format` | output file inspection |
| OP-PERF-003 | `ring_buffer`, `state_task`, `sd_task`, FreeRTOS core allocation | VAL-PERF-002 |
| OP-MSG-001 | `state_task`, `error_manager`, `ui_message` | message/error validation |

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

## 7. Message and Error Triggering Logic

| Condition / trigger | Error code | Display message | Recoverable by user | Triggering logic / source |
|---|---|---|---|---|
| Settings incomplete while READY | none | `NEED SETTINGS` | yes, by completing settings | `state_task` checks `settings_store` completeness |
| Calibration missing or expired while READY | none | `CAL REQUIRED` | yes, by Web calibration | `calibration_service` status consumed by `state_task` |
| Calibration plausibility fault | `ERR_CALIBRATION_FAULT` | `CAL FAULT` | no direct clear; retry calibration or setup clear can clear stored state | `calibration_service` / `calibration_store` |
| SD card missing | `ERR_SD_NO_CARD` | `NO SD`, then `SD OK/CLR` when recovered | yes | `sd_task` status / recovery path |
| SD free space below threshold | `ERR_SD_SPACE_LOW` | `SD LOW` | no, because archive does not free SD memory | `sd_task` pre-open check or low-space during write |
| SD file-count threshold reached | `ERR_SD_FILES_FULL` | `SD FULL (FILES)` in orange | yes, by Web file maintenance if SD free space is not low | `sd_task` status check; message does not force main page |
| Unexpected SD I/O fault | `ERR_SD_FAULT` | `SD ERROR` | no | `sd_task` write/flush/open/close failure classification |
| Accelerometer read failure | `ERR_ACCEL_NO_RESPONSE` | `ACCEL ERR` | no | `state_task` recording acquisition retries exhausted |
| Ring-buffer overflow | `ERR_RINGBUFFER_OVERFLOW` | `REC FAIL` | no | `ring_buffer_push()` failure during recording |
| RTC invalid/fault | `ERR_RTC_INVALID` | `RTC ERROR` | no | RTC/date-time validation path |
| PMU fault | `ERR_PMU_FAULT` | `PMU ERROR` | no | PMU/power status path |
| Touch fault | `ERR_TOUCH_FAULT` | `TOUCH ERROR` | no | touch driver/service path |
| Low battery | none | `LOW BATT` | device shutdown path | power/battery status consumed by `state_task` |
| USB lost while READY | none | `USB LOST` / shutdown path | not applicable | USB loss edge consumed by `state_task` |
| Shutdown requested | none | `SHUTDOWN` | not applicable | state transition to OFF |

Notes:

- SD recoverable errors use a two-step recovery: the physical condition must be corrected, then the operator acknowledges `SD OK/CLR`.
- Non-recoverable active errors remain latched until shutdown/service action or a later design adds a safe recovery path.
- Setup-lock messages are not active errors; they keep the recorder in READY and allow MENU access.

## 8. Recording Block Definitions

### 8.1 Block Summary

| Block ID | Name | Size | Status |
|---:|---|---:|---|
| `0x70` | Acceleration block | 13 bytes | implemented |
| `0x71` | Status/close block | 13 bytes | implemented |
| `0x72` | Calibration block | 184 bytes | implemented and validated |

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
| 2 | `uint16` | size | block size, currently `184` |
| 4 | `uint32` | calibration_version | calibration record version |
| 8 | `uint16` | year | calibration year |
| 10 | `uint8` | month | calibration month |
| 11 | `uint8` | day | calibration day |
| 12 | `uint8` | hour | calibration hour |
| 13 | `uint8` | minute | calibration minute |
| 14 | `uint8` | second | calibration second |
| 15 | `float32[3]` | gain_x/y/z | calibration gains |
| 27 | `float32[3]` | offset_x/y/z_mg | calibration offsets, milli-g |
| 39 | `float32[6][3]` | face_mean_mg | raw face mean values, milli-g |
| 111 | `float32[6][3]` | face_stddev_mg | raw face standard deviations, milli-g |
| 183 | `uint8` | checksum | checksum over bytes 0-182 |

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

### 8.5 Expected File Sequence

A normal file sequence is:

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
| Continued recorded-file validation after binary format changes | required after each block-format change |
