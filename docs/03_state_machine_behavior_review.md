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

## 2. Review Method

Behavior was reviewed from the current implementation, primarily:

- `src/tasks/state_task.cpp`;
- `src/tasks/sd_task.cpp`;
- `src/services/calibration_service.cpp`;
- `src/services/error_manager.cpp`;
- `src/services/ring_buffer.cpp`;
- `src/services/record_format.cpp`;
- `src/drivers/accel_driver.cpp`.

This document should be updated whenever state-machine behavior changes.

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

Calibration sampling is no longer driven by Web polling. While a calibration session is active, `state_task` calls:

```cpp
calibration_session_service(now);
```

This lets calibration sampling run at the configured 50 ms period.

## 5. `ST_BOOT`

Purpose:

- initialize required hardware/services after power-up;
- permit READY with settings/calibration locks rather than failing boot for missing setup data;
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
- refreshes calibration status;
- displays setup-lock messages when needed;
- handles WiFi state indirectly through UI/Web task control;
- monitors SD task error status even while idle;
- monitors USB loss, low battery, and power-long shutdown request;
- detects record-start hold.

Setup lock behavior:

| Setup condition | READY behavior |
|---|---|
| settings incomplete | remain READY, show `NEED SETTINGS`, recording locked |
| calibration missing/expired | remain READY, show `CAL REQUIRED`, recording locked |
| calibration fault latched | remain READY, show `CAL FAULT`, recording locked |
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

Before opening a new recording file, SD task verifies:

- SD card presence;
- file-count limit;
- free-space threshold.

If any check fails, SD task raises the corresponding SD error and the high-level state machine transitions to error handling.

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

Calibration is serviced while the device is in READY with WiFi/Web available. Recording is locked while calibration is missing/expired/faulted or while setup is incomplete.

Calibration session behavior:

1. Web Start starts a RAM-only calibration session.
2. `state_task` periodically calls `calibration_session_service(now)`.
3. The calibration service reads raw acceleration using `accel_read_xyz_raw()`.
4. Samples outside the dominant-axis gravity tolerance reset the rolling window.
5. A 40-sample rolling window is evaluated.
6. If the full window has stddev above threshold, the window is reset.
7. If the full window is stable and within gravity tolerance, a face is detected from the dominant axis.
8. If this is the first current-session value for the face, it is stored.
9. If the face already has a current-session value, it is replaced only if the new candidate has lower stddev than the current-session value.
10. Stored/NVS values are not used for this selection decision.
11. Once all six faces are captured, gains/offsets are computed and displayed.
12. Web Save stores the new calibration in NVS and clears any calibration fault latch.

Calibration Web display behavior:

- face table: `Face | Status | Value | NVS value | Stddev | NVS stddev`;
- current/last updated face highlighted in blue;
- result area: `Calibration: Ready / Active / Done`, NVS date, and `Axis | Gain | NVS Gain | Offset | NVS Offset`.

## 15. Follow-up Items

No closed or PMU-managed items are listed here.

Current follow-up items:

| Item | Status |
|---|---|
| Final acceptance tolerance for 20 Hz sample-rate and jitter validation | needs defined acceptance criteria |
| Continued recorded-file validation after format changes | validate on hardware after each binary format change |



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

## 20. RTC Synchronization During Recording

The RTC/date-time cache is required before recording start to create the filename/timebase token and to check setup/calibration freshness. The cache continues to be refreshed during RECORDING so the active UI clock updates normally. Recording timestamps use the captured start time plus the monotonic ESP timer through `timebase`, so the RTC is not needed for each recorded sample. Periodic RTC/date-time cache refresh remains enabled during RECORDING to keep the active UI clock current; standby mode skips normal UI refresh.

## 21. Display Standby Sub-State

Display standby is not a high-level recorder state. It is a UI/display sub-state allowed only while the recorder remains in READY or RECORDING and the local UI is on the main display.

While in standby:

- normal UI refresh is skipped;
- touch/LVGL processing continues at a reduced rate;
- recorder state logic, recording acquisition, SD writing, low-power handling, and error/message handling continue normally.

The UI exits standby if touch/button/USB/message/error activity occurs or if the recorder leaves READY or RECORDING.

Touch remains enabled during RECORDING so the UI standby sub-state can wake from touch. Touch is disabled again when leaving RECORDING for STOPPING.

The UI does not enter standby from MENU, SETTINGS, or setting-edit pages. These pages keep the display active while the operator is navigating or editing setup data.

## 22. SD Low-Space / File-Count Maintenance

When SD low-space or SD max-file-count is detected while not recording, the condition blocks recording but does not force the high-level recorder into ERROR. READY remains active, MENU remains available, and Web file maintenance can be started. The SD task continues servicing file operations for these maintenance conditions and automatically returns to normal SD operation once the condition clears.

## 22. SD Max-File-Count Maintenance

When SD max-file-count is detected while not recording and free space is still available, the condition blocks recording but does not force the high-level recorder into ERROR. READY remains active, MENU remains available, and Web file maintenance can be started. SD low-space remains a blocking SD condition because archiving files to `/processed` does not free SD memory.

`MSG_SD_FULL_FILES` is included in the UI non-forcing menu-access lock set, the same way settings/calibration setup-lock messages are. This prevents the UI sync layer from forcing the MENU page back to main while file-count maintenance is needed.

`SD FULL (FILES)` is orange/amber because the operator can resolve it through MENU -> START WIFI -> Web archive. `SD LOW` remains a blocking condition because archive moves files to `/processed` and does not free SD memory.

## 23. SD Error Reclassification to File-Count Maintenance

During SD recovery, a previous SD error such as card removal can reclassify to
`ERR_SD_FILES_FULL` after the SD card is reinserted and acknowledged. Because
file-count maintenance requires MENU/START WIFI access, state_task exits
`ST_ERROR`, clears the active error latch, enters `ST_READY`, and publishes
`MSG_SD_FULL_FILES`. Recording remains blocked until the root file count is
reduced, but Web file archive is available.

## SD File-Management Timing and Download Behavior

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
