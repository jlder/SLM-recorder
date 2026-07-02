<!--
SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
Copyright (c) 2026 AgingGliders
-->

# State Machine Behavior Review

## 1. Purpose

This document records the code-grounded behavior of the recorder state machines. It is intended to support review of the operational requirements in `01_recorder_requirements.md`.

The focus is the implemented behavior of:

- `state_task`;
- `sd_task`;
- calibration session servicing that affects recorder readiness;
- recording start/stop/error/shutdown behavior;
- SD recording and recovery behavior.

## 2. Implementation Sources

This behavior description is based on the implemented source files:

- `src/tasks/state_task.cpp`;
- `src/tasks/sd_task.cpp`;
- `src/services/calibration_service.cpp`;
- `src/services/error_manager.cpp`;
- `src/services/ring_buffer.cpp`;
- `src/services/record_format.cpp`;
- `src/drivers/accel_driver.cpp`;
- `src/tasks/web_task.cpp`, for Web/AP lifecycle behavior that affects READY support functions.

Maintain this document together with the source files when state-machine behavior is revised.

## 3. `state_task` Overview

`state_task` owns the high-level recorder state. The implemented states are:

| State | Purpose |
|---|---|
| `ST_BOOT` | Initialize required hardware/services and wait for SD task readiness |
| `ST_READY` | Idle operational state with UI/menu/setup/calibration access and recording authorization checks |
| `ST_STARTING` | Request SD recording file open |
| `ST_RECORDING` | Acquire corrected acceleration samples and feed the recording path |
| `ST_STOPPING` | Request SD recording file close |
| `ST_ERROR` | Display/hold user-visible error and wait for recovery/clear/shutdown |
| `ST_OFF` | Delay briefly, then request device shutdown |

The state machine uses first-pass entry actions. Each state is structured as:

1. entry actions;
2. recurring actions;
3. state-change actions.

## 4. Periodic Work Owned by `state_task`

Each state-task loop performs recurring housekeeping, including:

- hardware status snapshots;
- USB edge detection;
- battery-low status update;
- date/time service synchronization when appropriate;
- calibration session service call.

Calibration sampling is driven by `state_task`. While a calibration session is active, `state_task` calls:

```cpp
calibration_session_service(now);
```

This lets calibration sampling run at the configured 50 ms period.

## 5. `ST_BOOT`

Purpose:

- initialize required hardware/services after power-up;
- permit READY with settings/calibration locks for missing setup data;
- monitor SD boot/recovery status.

Entry/recurring behavior:

- initializes PMU, RTC, touch, and accelerometer in a fixed order;
- evaluates settings availability;
- checks SD task status because BOOT may be entered after recovery as well as at initial startup.

Transitions:

| Condition | Transition |
|---|---|
| hardware ready and SD task closed | `ST_READY` |
| boot timeout while hardware still not ready | `ST_ERROR` |
| SD error detected | `ST_ERROR` |

Notes:

- Incomplete settings do not prevent READY.
- Missing/expired calibration does not prevent READY.
- Recording remains locked later in READY until settings/calibration are complete.

## 6. `ST_READY`

Purpose:

- idle operational state;
- menu/settings/WiFi/calibration access;
- recording authorization;
- shutdown handling when not recording.

Entry actions:

- clears shutdown-after-stop request;
- clears active error state;
- synchronizes date/time service with RTC;
- refreshes calibration status;
- initializes hold-button detectors for READY behavior;
- authorizes support SD file access only when Web is enabled.

Recurring actions:

- evaluates settings completeness;
- authorizes Web SD file-management only when READY and Web support is enabled;
- refreshes calibration status;
- displays setup-lock messages when needed;
- handles WiFi state indirectly through UI/Web task control;
- monitors SD task error status even while idle;
- monitors fresh READY-local USB loss edge, low battery, and power-long shutdown request;
- detects record-start hold.

Setup lock behavior:

| Setup condition | READY behavior |
|---|---|
| settings incomplete | remain READY, show `NEED SETTINGS`, recording locked |
| recorder calibration missing/expired | remain READY, show `REC CAL REQ`, recording locked |
| installation calibration missing | remain READY, show `INST CAL REQ`, recording locked |
| recorder calibration fault latched | remain READY, show `REC CAL FAULT`, recording locked |
| setup complete | show `READY`, recording may be authorized if other conditions are met |

Menu behavior:

- Setup-lock messages do not force the UI back to the main page.
- MENU/WiFi access remains available so the operator can complete setup or retry calibration.
- The MENU button is orange during READY setup-lock conditions and blue during normal READY.

Transitions:

| Condition | Transition |
|---|---|
| SD error active | `ST_ERROR` |
| USB loss edge while READY | `ST_OFF` |
| low battery on battery power | `ST_OFF` or `ST_STOPPING` before `ST_OFF` |
| power button held for shutdown threshold | `ST_OFF` |
| settings clear gesture succeeds | `ST_OFF` |
| settings complete, calibration valid, and record start hold qualified | `ST_STARTING` |

## 7. `ST_STARTING`

Purpose:

- request creation/opening of the recording file.

Entry actions:

- capture date/time/monotonic token for filename/metadata;
- reset/prepare recording path;
- request SD open.

Recurring/transition behavior:

| Condition | Transition |
|---|---|
| SD file open confirmed | `ST_RECORDING` |
| SD/open error detected | `ST_ERROR` |
| startup/open timeout | `ST_ERROR` |

Notes:

- SD task owns file-count, card presence, and free-space checks before open.
- Calibration block `0x72` is written immediately after the recording file is opened and before acceleration data.

## 8. `ST_RECORDING`

Purpose:

- acquire corrected acceleration samples and feed the recording path.

Recurring actions:

- reads acceleration using the normal corrected accelerometer path;
- retries bounded acceleration reads;
- formats acceleration data;
- pushes formatted records to the ring buffer;
- monitors non-SD active errors;
- monitors SD task errors;
- checks stop/shutdown conditions.

Transitions:

| Condition | Transition |
|---|---|
| acceleration-read failure | `ST_ERROR` |
| ring-buffer overflow | `ST_ERROR` |
| SD task error | `ST_ERROR` |
| record stop hold | `ST_STOPPING` |
| power shutdown hold | `ST_STOPPING`, then `ST_OFF` after close |
| low battery on battery power | `ST_STOPPING`, then `ST_OFF` after close |

Notes:

- Calibration uses raw samples only during calibration sessions.
- Recording uses corrected samples through the normal accelerometer read path.

## 9. `ST_STOPPING`

Purpose:

- request SD file close and wait for SD task to report closed or error.

Entry actions:

- sends SD close request.

Transitions:

| Condition | Transition |
|---|---|
| SD file closed and normal stop | `ST_READY` |
| SD file closed and shutdown-after-stop requested | `ST_OFF` |
| SD close error | `ST_ERROR` |
| stop timeout | `ST_ERROR` |

Notes:

- Power-long and low-battery during recording intentionally close the file before shutdown.

## 10. `ST_ERROR`

Purpose:

- hold an active error message until the condition is recoverable and cleared, or until shutdown is requested.

Entry behavior:

- disables support SD file authorization;
- leaves the error-manager active message visible.

Recurring actions:

- updates whether the error is clearable;
- handles clear request from the power/clear button;
- handles SD-specific two-step recovery;
- permits shutdown request where applicable.

Clear behavior:

| Error type | Clear behavior |
|---|---|
| recoverable non-SD error | clear directly when power/clear hold is detected |
| SD error | wait for SD task recovery status, show `SD OK/CLR`, acknowledge clear, then return READY |
| non-clearable/fatal error | remain in ERROR until shutdown/service action |

## 11. `ST_OFF`

Purpose:

- display shutdown/off state briefly and request device shutdown.

Behavior:

- disables support SD file authorization;
- delays briefly;
- requests shutdown through device/power service.

## 12. `sd_task` Overview

`sd_task` owns SD card and recording file lifecycle. It exposes narrow status/request APIs consumed by `state_task`.

General responsibilities:

- detect SD presence and mount status;
- count recording files;
- check free space;
- open recording file on request;
- write recording blocks;
- close recording file on request or error path;
- expose recoverable SD error status.

## 13. SD Recording Behavior

### 13.1 Before open

Before opening a recording file, SD task verifies:

- SD card presence;
- file-count limit;
- free-space threshold.

If any check fails, SD task raises the corresponding SD error and the high-level state machine transitions to error handling.

For file opening, SD task requests a daily prefix from `record_format` using the registration and recording-start date. It then calls `sd_open_record_daily()`. The SD storage layer creates `_1.bin` for the first session of the day or renames the existing same-day file to the next `_N.bin` suffix and appends the session data.

### 13.2 During writing

While writing, SD task:

- drains ring-buffer data;
- writes to the open file;
- detects write failure or storage conditions;
- detects low-space while writing.

If low-space is detected while writing, the task follows the normal close path before reporting the low-space condition so the file is closed cleanly where possible.

### 13.3 Closing

On close request, SD task:

- writes final status/close information as currently implemented;
- closes the file;
- reports closed state or SD error.

## 14. Calibration Session Behavior

Calibration is serviced while the device is in READY with WiFi/Web available. Recording is locked while calibration is missing/expired/faulted or while setup is incomplete. While WiFi/Web is active, the screen START RECORD action is disabled; the physical RECORD button remains an independent hardware control and can start recording after READY exit cleanup turns WiFi/Web OFF.

Recorder sensor calibration session behavior:

1. Web Start starts a RAM-only calibration session.
2. `state_task` periodically calls `calibration_session_service(now)`.
3. The calibration service reads raw acceleration using `accel_read_xyz_raw()`.
4. Samples outside the dominant-axis gravity tolerance reset the rolling window and current-face sample count.
5. When the detected face changes, the current-face sample count and rolling window reset. A 40-sample rolling window is evaluated on the current face.
6. If the full window has stddev above threshold, the window is reset.
7. If the full window is stable and within gravity tolerance, a face is detected from the dominant axis.
8. If this is the first current-session value for the face, it is stored.
9. If the face already has a current-session value, it is replaced only if the new candidate has lower dominant-axis stddev than the current-session value. For +X/-X faces this means X stddev, for +Y/-Y this means Y stddev, and for +Z/-Z this means Z stddev. Off-axis stddev is still used for the stability check but does not decide whether a face capture improves.
10. Stored/NVS values are not used for this selection decision.
11. A valid face capture does not reset the rolling window. The service continues evaluating overlapping windows until the operator moves the recorder, the window becomes unstable, or the session ends.
12. Once all six faces are captured, gains/offsets are computed and displayed.
13. Web Save stores the accepted calibration in NVS and clears any recorder calibration fault latch.

Calibration Web display behavior:

- progress area: status, session state, current face, samples processed on that face, sensor temperature, lowest stddev for that face, best update count for that face, and time since the last best update;
- face summary: compact six-face status using `OK` for captured faces, `ACTIVE` for the currently detected/sampled face, and `—` for missing faces;
- states are intentionally simple: `ACTIVE` for the currently detected/sampled face, `OK` for captured faces, and `—` for missing faces;
- result area: `Calibration: Ready / Active / Done`, NVS date, and `Axis | Gain | NVS Gain | Offset | NVS Offset`.

## 15. Open Validation Items

The following validation items remain open for the controlled baseline:

| Item | Status |
|---|---|
| Final acceptance tolerance for 20 Hz sample-rate and jitter validation | needs defined acceptance criteria |
| Recorded-file validation after any block-format revision | validate on hardware after each binary format revision |



## 16. Performance-Relevant State Behavior

The recording performance requirement is 20 Hz acceleration acquisition with signed 16-bit acceleration values.

The architecture uses:

```text
core 0: corrected acceleration acquisition in state_task
ring_buffer for decoupling
core 1: sd_task for SD writing
```

This separation is intended to minimize acquisition jitter by avoiding direct dependence on SD-card write latency.

## 17. Recorded File Validation Note

Recorded file `FCFAG_20260517_222418.bin` validated the expected block sequence (`0x72`, repeated `0x70`, final `0x71`) and showed exact 50 ms timestamp intervals for 20 Hz in that run.

## 18. Global Low-Power Shutdown Behavior

Low-power shutdown is serviced globally after hardware housekeeping. When running on battery with no USB power present, a low-power condition transitions the recorder toward shutdown from any state. If recording is active or file opening is in progress, state_task requests SD close and uses the STOPPING path before OFF. If no file is open, the recorder transitions directly to OFF.

## 19. RTC Synchronization During Recording

The RTC/date-time cache is required before recording start to create the filename/timebase token and to check setup/calibration freshness. The cache continues to be refreshed during RECORDING so the active UI clock updates normally. Recording timestamps use the captured start time plus the monotonic ESP timer through `timebase`, so the RTC is not needed for each recorded sample. Periodic RTC/date-time cache refresh remains enabled during RECORDING to keep the active UI clock current; standby mode skips normal UI refresh.

## 20. Display Standby Sub-State

Display standby is not a high-level recorder state. It is a UI/display sub-state that may replace any normal recorder UI page after the configured inactivity timeout. It is not limited to the main page and is not blocked only because a particular READY/RECORDING/setup message is active. The low-battery shutdown notice is excluded from standby.

While in standby:

- the AMOLED display output is switched off;
- the display panel supply controlled by `LCD_EN` is switched off;
- normal UI refresh is skipped;
- touch/LVGL processing continues at a reduced rate;
- the previous UI page is remembered so it can be restored on wake;
- recorder state logic, recording acquisition, SD writing, low-power handling, and error/message handling continue normally.

The UI exits standby when touch activity is detected, when the power/clear or record button is pressed, or when USB power is inserted. During RECORDING, touch remains enabled so the UI standby sub-state can wake from touch while acquisition and SD writing continue.

### SD Free-Space Hysteresis

`SD_BOOT`, `SD_IDLE`, and the final pre-open guard use `SD_RECORD_START_MIN_FREE_MB` when deciding whether recording may start. This threshold is intentionally higher than the low-space threshold used during active recording.

While `SD_WRITING` is active, the SD storage layer updates a cached free-space estimate after each successful write. If the cached estimate falls below `SD_RECORD_LOW_FREE_MB`, the write path reports `ERR_SD_SPACE_LOW`; `sd_task` then transitions through `SD_CLOSING` so the file is closed using the normal low-space close path.

## 21. SD Low-Space / File-Count Maintenance

When SD max-file-count is detected while not recording and free space is still above `SD_RECORD_START_MIN_FREE_MB`, the condition blocks recording but does not force the high-level recorder into ERROR. READY remains active, MENU remains available, and Web file maintenance can be started. SD low-space remains a blocking SD condition because archiving files to `/processed` does not free SD memory.

`MSG_SD_FULL_FILES` is included in the UI non-forcing menu-access lock set, the same way settings/calibration setup-lock messages are. This prevents the UI sync layer from forcing the MENU page back to main while file-count maintenance is needed.

`SD FULL (FILES)` is orange/amber because the operator can resolve it through MENU -> START WIFI -> Web archive. `SD LOW` remains a blocking condition because archive moves files to `/processed` and does not free SD memory.

## 22. SD Error Reclassification to File-Count Maintenance

During SD recovery, a previous SD error such as card removal can reclassify to
`ERR_SD_FILES_FULL` after the SD card is reinserted and acknowledged. Because
file-count maintenance requires MENU/START WIFI access, state_task exits
`ST_ERROR`, clears the active error latch, enters `ST_READY`, and publishes
`MSG_SD_FULL_FILES`. Recording remains blocked until the root file count is
reduced, but Web file archive is available.


## 23. Installation Calibration State Behavior

Installation calibration is controlled from the Web interface while the recorder is in READY and Web support is enabled. Starting an installation calibration requires a valid sensor calibration because the installation workflow uses sensor-corrected samples as its input.

During the installation session, the calibration service maintains a rolling sample window. When the standard deviation is below the configured stability threshold and the measured gravity magnitude is within `INSTALLATION_GRAVITY_TOL_PCT`, the service computes a 3 x 3 matrix that rotates the measured gravity vector to +Z. The installation quality metric is the quadratic sum `sqrt(stddev_x^2 + stddev_y^2 + stddev_z^2)`. Unlike sensor calibration, installation calibration does not keep the lowest-noise candidate seen since session start. Every stable rolling window becomes the current installation candidate, including mean, stddev, quality, update counters, and matrix. Save is allowed only while the current rolling window is stable; invalid samples, unstable windows, invalid mean gravity, or matrix-computation failure reset the window and invalidate Save until a new stable window is available.

Sensor calibration preview/result endpoints compute candidate gains and offsets without saving to NVS and without latching recorder calibration faults. Only the save path is allowed to persist the calibration or latch a calibration plausibility fault.

Saving the installation calibration stores the matrix in NVS as the installation-calibration part of the active calibration record and applies it to normal accelerometer reads. Recording remains blocked until both the sensor calibration and installation calibration are valid. Sensor calibration and installation calibration have independent validity and timestamp fields.

## 24. Web/AP Lifecycle Behavior

`web_task` uses a server-once lifecycle. The `AsyncWebServer` object and route table are created once, and `s_server->begin()` is called only for the first Web ON cycle. Later Web ON cycles only restart the AP.

On Web OFF, `web_task` performs Web-side cleanup, ends any SD download session, clears Web locks, aborts any active OTA state, and stops the AP. It intentionally does not call `AsyncWebServer::end()` and does not delete/recreate the server. The port-80 AsyncWebServer listener is treated as a process-lifetime object because the selected AsyncWebServer/AsyncTCP stack does not provide a reliable stop/restart lifecycle for that listener after HTTP traffic.

The permanent `/diag` route is used as a lightweight health check during validation and troubleshooting.

## 25. SD File-Management Timing and Download Behavior

When the recorder is in READY and Web file-management is authorized, SD support operations are serviced from `SD_IDLE`. Recording states do not service Web/UI file-management operations.

To improve Web download responsiveness, `sd_task` uses a shorter file-operation polling period only when both conditions are true:

```text
s_sd_state == SD_IDLE
sd_files_is_authorized() == true
```

In all other SD states, the normal `SD_TASK_PERIOD_MS` period is used.

Web downloads use a sequential SD-owned session. The Web task does not open SD files directly. Instead:

1. `web_task` starts the session through `sd_files_download_begin()`;
2. `sd_task` opens the file through `sd_storage_download_begin()`;
3. each HTTP chunk is serviced through `sd_files_download_read()`;
4. `sd_storage_download_read()` reads the next sequential bytes from the open file;
5. transfer cleanup calls `sd_files_download_end()`, which closes the SD-owned file handle.

While a download session is active, the SD idle reprobe is skipped so the open download file handle is not invalidated by an SD reinitialization.

## 26. Web Download and Flight-Time Analysis Behavior

Web download uses the SD-file download path to transfer the selected daily `.bin` to the browser. The browser saves the `.bin` file and analyzes the same buffer locally to detect flight times. The displayed result is limited to the flight-time table and the sample-period average/standard deviation. Kossira occurrence/load-factor processing and CSV generation are intentionally disabled in this release baseline.

## 27. Web OTA Firmware Update Behavior

Firmware update is available only through the Web interface. Since Web support is enabled from READY, firmware update is operationally separated from active recording. The Web Firmware Update page expects a recorder application binary named like `SLM_recorder_date_version.bin`.

The Web Firmware Update tab provides a file selector and upload button. Browser-side upload progress is shown using the upload progress event.

`web_task` handles `POST /api/ota` uploads in chunks. At the first chunk, the handler checks that USB power is present and that the uploaded filename is an application `.bin`. If the check fails, the upload is rejected and the current firmware remains active.

During upload, received chunks are written through the ESP32 Arduino `Update` API. If write or finalization fails, the update is aborted and the current firmware remains active. If finalization succeeds, the Web response reports success and `web_task` restarts the recorder.

